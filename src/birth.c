/* File: birth.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "blitz.h"
#include "fs/path.h"
#include "log/log.h"
#include "player/killer.h"
#include "sdl-config.h"
#include "z-term.h"
#include "metarun.h"

/* Three-column layout constants (same as cmd4.c) */
#define COL_SKILL 2
#define COL_ABILITY 15
#define COL_DESCRIPTION 25

/* Forward declaration of wipe_screen_from function */
extern void wipe_screen_from(int col);

/* Locations of the tables on the screen */
#define HEADER_ROW 0
#define QUESTION_ROW 1
#define TABLE_ROW 2
#define DESCRIPTION_ROW 15
#define INSTRUCT_ROW 22

#define QUESTION_COL 2
#define RACE_COL 2
#define RACE_AUX_COL 19
#define CLASS_COL 17
#define CLASS_AUX_COL 27
#define TOTAL_AUX_COL 35
#define INVALID_CHOICE 255

static int find_named_artifact_for_character(void);
static void grant_starting_artifact(void);
static bool starting_artifact_is_eligible(int art_idx, int k_idx);

static void copy_start_items(start_item dest[MAX_START_ITEMS],
    const start_item src[MAX_START_ITEMS]);
static void replace_start_food(start_item list[MAX_START_ITEMS], byte from_sval,
    byte to_sval);

#define BLITZ_MAX_EFFECT_COUNT 9

static bool starting_artifact_is_eligible(int art_idx, int k_idx)
{
    artefact_type *a_ptr;
    object_type object_type_body;
    object_type *o_ptr = &object_type_body;

    if (art_idx <= 0 || art_idx >= z_info->art_max)
        return false;

    a_ptr = &a_info[art_idx];
    if (!a_ptr->name[0])
        return false;

    if (a_ptr->level > 10)
        return false;

    if (!k_idx)
        return false;

    object_prep(o_ptr, k_idx);
    o_ptr->name1 = art_idx;
    apply_magic(o_ptr, -1, true, true, true, true);

    return (object_smithing_difficulty(o_ptr) <= 45);
}

/* Character ability names */
static const char *character_ability_names[S_MAX][ABILITIES_MAX] =
{
    [S_MEL] = {
        [MEL_POWER]            = "Power",
        [MEL_FINESSE]          = "Finesse",
        [MEL_KNOCK_BACK]       = "Knock Back",
        [MEL_THROWING]         = "Throwing",
        [MEL_POLEARMS]         = "Polearm Mastery",
        [MEL_CHARGE]           = "Charge",
        [MEL_FOLLOW_THROUGH]   = "Follow-Through",
        [MEL_IMPALE]           = "Impale",
        [MEL_CONTROL]          = "Subtlety",
        [MEL_WHIRLWIND_ATTACK] = "Whirlwind Attack",
        [MEL_ZONE_OF_CONTROL]  = "Zone of Control",
        [MEL_SMITE]            = "Smite",
        [MEL_TWO_WEAPON]       = "Two Weapon Fighting",
        [MEL_RAPID_ATTACK]     = "Rapid Attack",
        [MEL_STR]              = NULL,  /* if you care about STR */
    },
    [S_ARC] = {
        [ARC_ROUT]        = "Rout",
        [ARC_FLETCHERY]   = "Fletchery",
        [ARC_POINT_BLANK] = "Point Blank Archery",
        [ARC_PUNCTURE]    = "Puncture",
        [ARC_AMBUSH]      = "Ambush",
        [ARC_VERSATILITY] = "Versatility",
        [ARC_CRIPPLING]   = "Crippling Shot",
        [ARC_DEADLY_HAIL] = "Deadly Hail",
        [ARC_DEX]         = NULL,
    },
    [S_EVN] = {
        [EVN_DODGING]            = "Dodging",
        [EVN_BLOCKING]           = "Blocking",
        [EVN_PARRY]              = "Parry",
        [EVN_CROWD_FIGHTING]     = "Crowd Fighting",
        [EVN_LEAPING]            = "Leaping",
        [EVN_SPRINTING]          = "Sprinting",
        [EVN_FLANKING]           = "Flanking",
        [EVN_HEAVY_ARMOUR]       = "Heavy Armour Use",
        [EVN_RIPOSTE]            = "Riposte",
        [EVN_CONTROLLED_RETREAT] = "Controlled Retreat",
        [EVN_DEX]                = NULL,
    },
    [S_STL] = {
        [STL_DISGUISE]          = "Disguise",
        [STL_ASSASSINATION]     = "Assassination",
        [STL_CRUEL_BLOW]        = "Cruel Blow",
        [STL_EXCHANGE_PLACES]   = "Exchange Places",
        [STL_OPPORTUNIST]       = "Opportunist",
        [STL_VANISH]            = "Vanish",
        [STL_DEX]               = NULL,
    },
    [S_PER] = {
        [PER_QUICK_STUDY]    = "Quick Study",
        [PER_FOCUSED_ATTACK] = "Focused Attack",
        [PER_KEEN_SENSES]    = "Keen Senses",
        [PER_CONCENTRATION]  = "Concentration",
        [PER_ALCHEMY]        = "Alchemy",
        [PER_BANE]           = "Bane",
        [PER_OUTWIT]         = "Outwit",
        [PER_LISTEN]         = "Resonance",
        [PER_MASTER_HUNTER]  = "Master Hunter",
        [PER_GRA]            = NULL,
    },
    [S_WIL] = {
        [WIL_CURSE_BREAKING]        = "Curse Breaking",
        [WIL_CHANNELING]            = "Channeling",
        [WIL_STRENGTH_IN_ADVERSITY] = "Strength in Adversity",
        [WIL_FORMIDABLE]            = "Formidable",
        [WIL_INNER_LIGHT]           = "Inner Light",
        [WIL_INDOMITABLE]           = "Indomitable",
        [WIL_OATH]                  = "Oath",
        [WIL_POISON_RESISTANCE]     = "Poison Resistance",
        [WIL_VENGEANCE]             = "Vengeance",
        [WIL_MAJESTY]               = "Majesty",
        [WIL_CON]                   = NULL,
    },
    [S_SMT] = {
        [SMT_WEAPONSMITH]   = "Weaponsmith",
        [SMT_ARMOURSMITH]   = "Armoursmith",
        [SMT_JEWELLER]      = "Jeweller",
        [SMT_ENCHANTMENT]   = "Enchantment",
        [SMT_EXPERTISE]     = "Expertise",
        [SMT_ARTEFACT]      = "Artifice",
        [SMT_MASTERPIECE]   = "Masterpiece",
        [SMT_ALLOY_MASTERY] = "Alloy mastery",
        [SMT_GRA]           = NULL,
    },
    [S_SNG] = {
        [SNG_ELBERETH]      = "Song of Elbereth",
        [SNG_CHALLENGE]     = "Song of Challenge",
        [SNG_DELVINGS]      = "Song of Delvings",
        [SNG_FREEDOM]       = "Song of Freedom",
        [SNG_SILENCE]       = "Song of Silence",
        [SNG_STAUNCHING]    = "Song of Staunching",
        [SNG_THRESHOLDS]    = "Song of Thresholds",
        [SNG_TREES]         = "Song of the Trees",
        [SNG_REVEALING]     = "Song of Revealing",
        [SNG_WOVEN_THEMES]  = "Woven Themes",
        [SNG_SLAYING]       = "Song of Slaying",
        [SNG_ELVENESS]      = "Song of Elveness",
        [SNG_STAYING]       = "Song of Staying",
        [SNG_DISGUISE]      = "Song of Disguise",
        [SNG_LORIEN]        = "Song of Lorien",
        [SNG_SHATTERING]    = "Song of Shattering",
        [SNG_MASTERY]       = "Song of Mastery",
        [SNG_CONTEST]       = "Song of Contest",
        [SNG_LAMENT]        = "Song of Lament",
        [SNG_GRA]           = NULL,
    },
    [S_SPC] = {
        [SPC_MANDOS] = "Mandos' Doom", /* immunity reward */
        [SPC_AULE] = "Aule's Forge", /* improved masterpiece forging */
        [SPC_OATH_MERCY] = "Oath of Mercy",
        [SPC_OATH_SILENCE] = "Oath of Silence",
        [SPC_OATH_IRON] = "Oath of Iron",
        [SPC_NIENA_MERCY] = "Niena's Gift of Mercy", /* Enhanced stealth from mercy quest */
        [SPC_OATH_SMITH] = "Oath of the Smith",
        [SPC_OATH_VALOROUS] = "Oath of the Valorous Heart",
        [SPC_UNIQUE_BANE] = "Unique Bane", /* Enhanced effectiveness against unique monsters */
        [SPC_OATH_LIGHT] = "Oath of Light",
    },
};

/*
 * Forward declare
 */
typedef struct birther birther;
typedef struct birth_menu birth_menu;

/*
 * A structure to hold "rolled" information
 */
struct birther
{
    s16b age;
    s16b wt;
    s16b ht;
    s16b sc;

    s16b stat[A_MAX];

    char history[550];
};

/*
 * A structure to hold the menus
 */
struct birth_menu
{
    bool ghost;
    cptr name;
    cptr text;
};

// s16b adj_c[A_MAX];

static int get_start_xp(void)
{
    if (birth_fixed_exp)
    {
        return PY_FIXED_EXP;
    }
    else
    {
        return PY_START_EXP;
    }
}

/* -----------------------------------------------------------
 * new: delegate to the (i386-safe) 2-bit accessor in metarun.h
 * --------------------------------------------------------- */
static int curse_count(int id)           /* 0-31 */
{
    return CURSE_GET(id);
}


/* Return net adjustment for a primary stat from EVERY active metarun curse */
static int curses_stat_adj(int s)   /* s = 0-3  (STR-DEX-CON-GRA) */
{
    int delta = 0;
    for (int bit = 0; bit < z_info->cu_max; bit++) {
        int cnt = curse_count(bit);
        if (cnt)
            delta += cnt * cu_info[bit].cu_adj[s];
    }
    return delta;
}



/*
 * Generate some info that the auto-roller ignores
 */
static void get_extra(void)
{
    int i, j;
    
    p_ptr->new_exp = p_ptr->exp = get_start_xp();
    p_ptr->discovery_lore_flags = 0;
    log_debug("Set starting experience to %d", p_ptr->exp);

    /* Player is not singing */
    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->song_target_idx = 0;
    p_ptr->song_target_song = SNG_NOTHING;
    p_ptr->song_lockout_timer = 0;
    p_ptr->song_contest_player_stacks = 0;
    p_ptr->song_duel_pad = 0;
    p_ptr->song_contest_last_turn = 0;
    
    /* Clear the abilities and add character abilities - but preserve oath abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            /* Preserve oath abilities (SPC_OATH_MERCY, SPC_OATH_SILENCE, SPC_OATH_IRON, SPC_OATH_SMITH, SPC_OATH_VALOROUS, SPC_OATH_LIGHT) */
            if (i == S_SPC && (j == SPC_OATH_MERCY || j == SPC_OATH_SILENCE || j == SPC_OATH_IRON || j == SPC_OATH_SMITH || j == SPC_OATH_VALOROUS || j == SPC_OATH_LIGHT))
            {
                /* Keep existing oath abilities intact */
                continue;
            }
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }
    
    /* Grant all parsed character abilities */
    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[p_ptr->pcharacter].a_adj[slot][0];
        /* sentinel: no more entries */
        if (stat < 0) break;

        int ab = c_info[p_ptr->pcharacter].a_adj[slot][1];
        /* sanity-check bounds */
        if (stat < S_MAX && ab < ABILITIES_MAX)
        {
            p_ptr->innate_ability[stat][ab] = true;
            p_ptr->active_ability[stat][ab] = true;
            log_debug("Assigned character ability: stat=%d, ability=%d", stat, ab);
        }
    }
}

/*
 * Clear all the global "character" data
 */
void player_wipe(void)
{
    /* We are about to wipe the old hero, so there is no fully-generated
     * character any more.  This must be cleared **before** we enter the
     * next character-creation cycle; otherwise helpers such as
     * show_scores() believe a character still exists. */
    character_generated = false;
    log_debug("birth.c: character_generated set to false - starting character wipe");
    int i;
    char history[550];
    int stat[A_MAX];

    log_debug("Wiping player data for new character creation");

    /* Backup the player choices */
    // Initialized to soothe compilation warnings
    byte prace = 0;
    byte pcharacter = 0;
    int age = 0;
    int height = 0;
    int weight = 0;

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        log_debug("Restoring previous character choices from dead character");
        /* Backup the player choices */
        prace = p_ptr->prace;
        pcharacter = p_ptr->pcharacter;
        age = p_ptr->age;
        height = p_ptr->ht;
        weight = p_ptr->wt;
        sprintf(history, "%s", p_ptr->history);

        for (i = 0; i < A_MAX; i++)
        {
            if (!(p_ptr->noscore & 0x0008))
                stat[i] = p_ptr->stat_base[i]
                    - (rp_ptr->r_adj[i] + current_character_profile->h_adj[i]);
            else
                stat[i] = 0;
        }
    }

    /* Wipe the player */
    memset(p_ptr, 0, sizeof(player_type));

    supplies_reset_store();

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        /* Restore the choices */
        p_ptr->prace = prace;
        p_ptr->pcharacter = pcharacter;
        p_ptr->game_type = 0;
        p_ptr->age = age;
        p_ptr->ht = height;
        p_ptr->wt = weight;
        sprintf(p_ptr->history, "%s", history);
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = stat[i];
        }
    }
    else
    {
        /* Reset */
        p_ptr->prace = 0;
        p_ptr->pcharacter = 0;
        p_ptr->game_type = 0;
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        p_ptr->history[0] = '\0';
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }

    /* Clear the inventory */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_wipe(&inventory[i]);
    }

    /* Start with no artefacts made yet */
    /* and clear the slots for in-game randarts */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        a_ptr->cur_num = 0;
        a_ptr->found_num = 0;
        a_ptr->seen = 0;
    }

    metarun_apply_artefact_memory();
    
    /* Initialize Valar artifact reservation array */
    if (!valar_reserved_artifacts)
    {
        valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
    }
    for (i = 0; i < z_info->art_max; i++)
    {
        valar_reserved_artifacts[i] = false;
    }

    /*re-set the object_level*/
    object_level = 0;

    /* Reset the "objects" */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Reset "tried" */
        k_ptr->tried = false;

        /* Reset "aware" */
        k_ptr->aware = false;
    }

    /* Reset the "monsters" */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Hack -- Reset the counter */
        r_ptr->cur_num = 0;

        /* Hack -- Reset the max counter */
        r_ptr->max_num = 100;

        /* Hack -- Reset the max counter */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            r_ptr->max_num = 1;

        /* Clear player sights/kills */
        l_ptr->psights = 0;
        l_ptr->pkills = 0;
    }

    /*No current player ghosts*/
    bones_selector = 0;

    // give the player the most food possible without a message showing
    p_ptr->food = PY_FOOD_FULL - 1;

    // reset the stair info
    p_ptr->stairs_taken = 0;
    p_ptr->staircasiness = 0;

    // reset the forge info
    p_ptr->fixed_forge_count = 0;
    p_ptr->forge_count = 0;

    // No vengeance at birth
    p_ptr->vengeance = 0;

    // Morgoth unhurt
    p_ptr->morgoth_state = 0;
    p_ptr->morgoth_second_wind = 0;

    p_ptr->killed_enemy_with_arrow = false;

    p_ptr->oath_type = 0;
    p_ptr->oaths_broken = 0;

    p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    p_ptr->tulkas_target_r_idx = 0;
    p_ptr->tulkas_prize_a_idx = 0;
    p_ptr->tulkas_quest_complete = 0;

    /* Aule quest init */
    p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
    p_ptr->aule_forge_y = 0;
    p_ptr->aule_forge_x = 0;
    p_ptr->aule_reserved = 0;
    p_ptr->aule_level = 0;
    p_ptr->aule_last_object_diff = 0;
    
    /* Mandos quest init */
    p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
    p_ptr->mandos_vault_y = 0;
    p_ptr->mandos_vault_x = 0;
    p_ptr->mandos_monsters_remaining = 0;
    p_ptr->mandos_level = 0;
    p_ptr->mandos_reserved = 0;
    
    /* Niena quest init */
    p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
    p_ptr->niena_level = 0;
    
    /* Orome quest init */
    p_ptr->orome_quest = OROME_QUEST_NOT_STARTED;
    p_ptr->orome_killed_count = 0;
    p_ptr->orome_target_type = 0;
    p_ptr->orome_target_count = 0;
    /* Varda quest init */
    p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
    p_ptr->varda_vault_ready = 0;
    p_ptr->varda_vault_placed = 0;
    p_ptr->varda_reserved = 0;
    p_ptr->varda_level = 0;
    
    p_ptr->quest_vault_used = 0;
    
    /* Quest states should always start at NOT_STARTED for new characters */
    /* Metarun completion is checked separately via metarun_is_quest_completed() */
    log_trace("Birth: All quest states initialized to NOT_STARTED for new character");
    for (i = 0; i < 15; i++) p_ptr->quest_reserved[i] = 0; /* quest_reserved[0] = any quest spawned flag; quest_reserved[1..6] = per-run quest completion markers */

    /*re-set the thefts counter*/
    recent_failed_thefts = 0;

    /*re-set the altered inventory counter*/
    allow_altered_inventory = 0;

    // reset some unique flags
    p_ptr->unique_forge_made = false;
    p_ptr->unique_forge_seen = false;
    for (i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        p_ptr->greater_vaults[i] = 0;
    }
}

/* ------------------------------------------------------------------
 * Hand out one start-item list (race or character template).
 * ------------------------------------------------------------------ */
static void give_start_items(const start_item *list)
{
    int i, slot, inven_slot;
    object_type object_type_body, *i_ptr, *o_ptr;

    for (i = 0; i < MAX_START_ITEMS && list[i].tval; i++)
    {
        const start_item *e_ptr = &list[i];

        /* Look up kind */
        s16b k_idx = lookup_kind(e_ptr->tval, e_ptr->sval);
        if (!k_idx) continue;

        object_kind *k_ptr = &k_info[k_idx];
        i_ptr = &object_type_body;

        /* Prepare object */
        object_prep(i_ptr, k_idx);
        i_ptr->number = (byte)rand_range(e_ptr->min, e_ptr->max);
        i_ptr->weight = k_ptr->weight;

        /* Where would this be wielded? */
        slot = wield_slot(i_ptr);

        /* Light sources start with their standard default fuel. */
        if (slot == INVEN_LITE)
        {
            if (i_ptr->sval == SV_LIGHT_TORCH)
                i_ptr->timeout = 1000;
            else if (i_ptr->sval == SV_LIGHT_LANTERN)
                i_ptr->timeout = (FUEL_LAMP * 2) / 5;
            else if (i_ptr->sval == SV_LIGHT_MALLORN)
                i_ptr->timeout = 100;
        }

        bool auto_identify = player_auto_identifies_object(i_ptr);
        bool start_known = true;
        if (!auto_identify
            && ((i_ptr->tval == TV_POTION)
                || (i_ptr->tval == TV_FOOD && i_ptr->sval <= SV_FOOD_SICKNESS)
                || (i_ptr->tval == TV_GEM)))
        {
            start_known = false;
        }

        if (auto_identify)
            ident(i_ptr);
        else if (start_known)
            object_known(i_ptr);

        /* inven_carry() may wipe supply items, so keep a copy for follow-up
         * handling such as auto-equipping the starting light. */
        object_type carry_obj;
        object_copy(&carry_obj, i_ptr);

        /* Carry it */
        int carry_slot = inven_carry(i_ptr, true);

        if (carry_slot == SUPPLIES_INDEX)
        {
            char name[80];
            object_desc(name, sizeof(name), &carry_obj, true, 3);
            char label = supplies_label_char();
            if (!label)
                label = 'a';
            log_info("Starting item went to supplies: %s (%c)", name, label);
            continue;
        }

        if (carry_slot < 0)
            continue;

        inven_slot = carry_slot;

        /* Auto-wield if slot empty */
        if (slot >= INVEN_WIELD && inventory[slot].tval == 0)
        {
            o_ptr = &inventory[slot];
            object_copy(o_ptr, i_ptr);

            if (o_ptr->tval != TV_ARROW) o_ptr->number = 1;

            inven_item_increase(inven_slot, -(o_ptr->number));
            inven_item_optimize(inven_slot);
            p_ptr->equip_cnt++;
        }

        object_wipe(i_ptr); /* avoid dupes */
    }
}

static void equip_starting_light_from_supply(void)
{
    int supply_idx;
    object_type equip_light;

    if (inventory[INVEN_LITE].tval != 0)
        return;

    supply_idx = -1;
    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        if (!supplies_is_light_object(o_ptr))
            continue;
        if (wield_slot(o_ptr) != INVEN_LITE)
            continue;
        supply_idx = i;
        break;
    }

    if (supply_idx < 0 || !supplies_take_one(supply_idx, &equip_light))
        return;

    object_copy(&inventory[INVEN_LITE], &equip_light);
    if (inventory[INVEN_LITE].sval == SV_LIGHT_LANTERN)
        inventory[INVEN_LITE].timeout = 0;
    p_ptr->equip_cnt++;
}

static void copy_start_items(start_item dest[MAX_START_ITEMS],
    const start_item src[MAX_START_ITEMS])
{
    int item_idx;

    for (item_idx = 0; item_idx < MAX_START_ITEMS; item_idx++)
    {
        dest[item_idx] = src[item_idx];
    }
}

static void replace_start_food(start_item list[MAX_START_ITEMS], byte from_sval,
    byte to_sval)
{
    int item_idx;

    for (item_idx = 0; item_idx < MAX_START_ITEMS && list[item_idx].tval;
         item_idx++)
    {
        if (list[item_idx].tval == TV_FOOD && list[item_idx].sval == from_sval)
        {
            list[item_idx].sval = to_sval;
        }
    }
}

/*
 * Find a named artifact matching the current character.
 * Returns artifact index if found, otherwise 0.
 * 
 * Matches artifacts with "of {CharacterName}" in their name.
 * For example: "Ring of Barahir" matches character "Barahir",
 *              "Crown of Feanor" matches character "Feanor".
 */
static int find_named_artifact_for_character(void)
{
    character_profile *current_character_profile = &c_info[p_ptr->pcharacter];
    const char *character_name = c_name + current_character_profile->name;
    
    /* Build pattern: "of {CharacterName}" */
    char pattern[64];
    char art_lower[MAX_LEN_ART_NAME];
    char pattern_lower[64];
    
    strnfmt(pattern, sizeof(pattern), "of %s", character_name);
    
    /* Convert pattern to lowercase for case-insensitive comparison */
    for (int i = 0; pattern[i] && i < (int)sizeof(pattern_lower) - 1; i++) {
        pattern_lower[i] = tolower((unsigned char)pattern[i]);
    }
    pattern_lower[strlen(pattern)] = '\0';
    
    /* Search all artifacts for one matching this character's name */
    for (int i = 1; i < z_info->art_max; i++) {
        artefact_type *a_ptr = &a_info[i];
        
        /* Skip artifacts without names or already created */
        if (!a_ptr->name[0]) continue;
        if (a_ptr->cur_num > 0) continue;
        if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;

        /* Convert artifact name to lowercase */
        for (int j = 0; a_ptr->name[j] && j < MAX_LEN_ART_NAME - 1; j++) {
            art_lower[j] = tolower((unsigned char)a_ptr->name[j]);
        }
        art_lower[strlen(a_ptr->name)] = '\0';
        
        /* Check if artifact name contains "of {CharacterName}" */
        if (strstr(art_lower, pattern_lower)) {
            /* Verify it's a valid base kind */
            int k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (starting_artifact_is_eligible(i, k_idx)) {
                log_info("Found named artifact for %s: %s (idx=%d)", 
                         character_name, a_ptr->name, i);
                return i;
            }
        }
    }
    
    log_debug("No named artifact found for character: %s", character_name);
    return 0;
}

static void grant_starting_artifact(void)
{
    int art_idx = 0;
    int k_idx = 0;
    
    /* First, try to find a named artifact for this character */
    art_idx = find_named_artifact_for_character();
    
    if (art_idx > 0) {
        /* Found a named artifact - validate and grant it */
        artefact_type *a_ptr = &a_info[art_idx];
        k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
        
        if (!k_idx) {
            log_warn("Named artifact has invalid base kind (idx=%d)", art_idx);
            art_idx = 0;  /* Fall through to random selection */
        } else if (valar_reserved_artifacts && valar_reserved_artifacts[art_idx]) {
            log_info("Named artifact already reserved (idx=%d)", art_idx);
            art_idx = 0;  /* Fall through to random selection */
        } else if (!starting_artifact_is_eligible(art_idx, k_idx)) {
            log_info("Named artifact does not meet starting thresholds (idx=%d)", art_idx);
            art_idx = 0;  /* Fall through to random selection */
        }
    }
    
    /* If no named artifact, use the original random selection logic */
    if (art_idx == 0) {
        int candidates[512];
        int candidate_kinds[512];
        int count = 0;
        for (int i = 1; i < z_info->art_max && count < (int)N_ELEMENTS(candidates); i++) {
            artefact_type *a_ptr = &a_info[i];
            int k;

            if (!a_ptr->name[0]) continue;
            if (a_ptr->cur_num > 0) continue;
            if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;

            k = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (!starting_artifact_is_eligible(i, k)) continue;

            candidates[count] = i;
            candidate_kinds[count] = k;
            count++;
        }

        if (count == 0) {
            log_warn("No artefacts available for starting blessing under the lvl<=10 and difficulty<=45 filter.");
            msg_print("No artefact could be granted.");
            return;
        }

        int pick = rand_int(count);
        art_idx = candidates[pick];
        k_idx = candidate_kinds[pick];
    }
    
    /* Grant the selected artifact */
    artefact_type *a_ptr = &a_info[art_idx];

    object_type object_type_body;
    object_type *o_ptr = &object_type_body;
    object_prep(o_ptr, k_idx);
    o_ptr->name1 = art_idx;
    apply_magic(o_ptr, -1, true, true, true, true);
    object_aware(o_ptr);
    object_known(o_ptr);
    int slot = inven_carry(o_ptr, true);
    if (slot < 0) {
        log_warn("Starting artefact could not be carried (idx=%d)", art_idx);
        msg_print("You have no room for a starting artefact.");
        return;
    }
    a_ptr->cur_num = 1;
    if (valar_reserved_artifacts) valar_reserved_artifacts[art_idx] = true;

    log_info("Starting artefact granted: %s (idx=%d)", a_ptr->name, art_idx);
}

static void player_outfit(void)
{
    /* ---------- locals ---------- */
    time_t      c;
    struct tm  *tp;

    log_debug("Starting player equipment setup");

    /* skip all starting-gear on load */
    if (character_loaded) return;

    /* ---------- escape-curse check ---------- */
    if (curse_flag_count_cur(CUR_NOSTART)) return;

    /* ---------- pointers into info arrays ---------- */
    player_race  *rp_ptr = &p_info[p_ptr->prace];
    character_profile *current_character_profile = &c_info[p_ptr->pcharacter];
    start_item race_start_items[MAX_START_ITEMS];

    copy_start_items(race_start_items, rp_ptr->start_items);

    if (current_character_profile->flags_u & UNQ_SMT_EOL)
    {
        replace_start_food(race_start_items, SV_FOOD_LEMBAS, SV_FOOD_BREAD);
    }

    /* ---------- hand out gear ---------- */
    log_debug("Giving starting items for race: %s", p_name + rp_ptr->name);
    give_start_items(race_start_items);   /* race first  */
    log_debug("Giving starting items for character: %s", c_name + current_character_profile->name);
    give_start_items(current_character_profile->start_items);   /* character kit */

    if (!run_mode_is_blitz()
        && metarun_has_major_blessing_effect(METARUN_MAJOR_EFFECT_START_ARTIFACT)) {
        grant_starting_artifact();
    }

    /* Starting light sources are stored in supplies now; equip one from there
     * after all birth gear has been added. */
    equip_starting_light_from_supply();

    /* ---------- Christmas present (unchanged) ---------- */
    c  = time((time_t*)0);
    tp = localtime(&c);
    if ((tp->tm_mon == 11) && (tp->tm_mday >= 25))
    {
        object_type object_type_body, *i_ptr = &object_type_body;

        s16b k_idx = lookup_kind(TV_CHEST, SV_CHEST_PRESENT);
        object_prep(i_ptr, k_idx);
        i_ptr->number = 1;
        i_ptr->pval   = -20;

        (void)inven_carry(i_ptr, true);
    }

    /* ---------- bookkeeping ---------- */
    p_ptr->update |= (PU_BONUS | PU_MANA);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);

    log_debug("Player equipment setup completed");
}

/*
 * Clear the previous question
 */
static void clear_question(void)
{
    int i;

Term_erase(TOTAL_AUX_COL, 0, 255);

    for (i = QUESTION_ROW; i < TABLE_ROW; i++)
    {
        /* Clear line, position cursor */
        Term_erase(0, i, 255);
    }
}

static void birth_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static bool birth_pending_compact_description_confirm = false;
static int birth_prompt_row(void);
static void birth_put_str_fit(byte attr, cptr text, int row, int col);

static bool birth_confirm_input(int ch, bool steamdeck)
{
    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == INPUT_BIND_CONFIRM)
        return true;

    if (steamdeck && ch == steamdeck_confirm_key())
        return true;

    return false;
}

static bool birth_confirm_unspent_stat_points(int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    int message_row;
    int prompt_row;
    char key;
    char buf[160];
    bool confirmed = false;

    if (points_left <= 0)
        return true;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    prompt_row = birth_prompt_row();
    message_row = prompt_row - 1;
    if (message_row < 0)
        message_row = 0;

    Term_erase(0, message_row, 255);
    Term_erase(0, prompt_row, 255);

    strnfmt(buf, sizeof(buf), "Unused stat points: %d. Continue anyway?", points_left);
    birth_put_str_fit(TERM_YELLOW, buf, message_row, 1);

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));

        strnfmt(buf, sizeof(buf), "%s continue  %s keep allocating",
            confirm_label, back_label);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "SPACE/y continue  n/ESC keep allocating");
    }

    birth_put_str_fit(TERM_SLATE, buf, prompt_row, 1);
    ui_scroll_area_clear();
    ui_menu_click_begin();
    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, confirm_label);
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "continue");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, back_label);
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "keep allocating");
    }
    else
    {
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "SPACE/y");
        ui_menu_click_add_text_token(1, 1, prompt_row, buf, "continue");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "n/ESC");
        ui_menu_click_add_text_token(0, 1, prompt_row, buf, "keep allocating");
    }
    sdl_touch_pane_begin_yes_no_prompt(
        "Continue with unused stat points?");
    Term_fresh();

    while (1)
    {
        int clicked_choice = -1;

        hide_cursor = true;
        key = inkey();
        hide_cursor = false;

        if (ui_menu_click_take(&clicked_choice))
        {
            confirmed = (clicked_choice != 0);
            break;
        }

        if (birth_confirm_input(key, steamdeck))
        {
            confirmed = true;
            break;
        }
        if (steamdeck && key == steamdeck_back_key())
            break;

        if (key == 'y' || key == 'Y')
        {
            confirmed = true;
            break;
        }
        if (key == 'n' || key == 'N' || key == ESCAPE)
            break;

        bell("Confirm or cancel the stat warning.");
    }

    sdl_touch_pane_end_yes_no_prompt();
    ui_menu_click_clear();
    return confirmed;
}

static bool birth_show_compact_description_after_assignment(bool steamdeck)
{
    char ch;
    char buf[160];

    while (1)
    {
        int wid = 80;
        int hgt = 24;
        int prompt_row;

        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        display_player(100);

        prompt_row = hgt - 1;
        if (prompt_row < 0)
            prompt_row = 0;
        Term_erase(0, prompt_row, 255);

        if (steamdeck)
        {
            char confirm_label[16];
            char back_label[16];

            birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
            birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
            strnfmt(buf, sizeof(buf), "%s back  %s continue", back_label, confirm_label);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "ESC back to assignment  SPACE/ENTER continue");
        }

        c_put_str(TERM_SLATE, buf, prompt_row, 1);

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;

        if ((ch == ESCAPE) || (ch == '4') || (ch == 'q') || (ch == 'Q'))
            return false;

        if (birth_confirm_input(ch, steamdeck) || (ch == '6'))
            return true;
    }
}

static int character_choice_index_by_name(cptr choice_name)
{
    int character_idx;

    if (!choice_name)
        return -1;

    for (character_idx = 0; character_idx < z_info->c_max; character_idx++)
    {
        if (!strcmp(choice_name, c_name + c_info[character_idx].name))
            return character_idx;
    }

    return -1;
}

static int birth_prompt_row(void);
static int birth_description_base_row(void);

static bool character_description_has_room(void)
{
    int min_description_rows = 8;
    int available_rows = birth_prompt_row() - birth_description_base_row();

    return (available_rows >= min_description_rows);
}

static bool character_selection_tight_height(void)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    return (hgt <= 18);
}

static bool character_flags_need_compact_layout(void)
{
    int wid = 80;
    int hgt = 24;
    int min_flags_wid = TOTAL_AUX_COL + 21 + 20;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;

    return (wid < min_flags_wid) || character_selection_tight_height();
}

static cptr character_selection_header_text(bool character_phase)
{
    if (character_phase && character_flags_need_compact_layout())
        return "Character:";

    return "Character Selection:";
}

static void draw_character_selection_header(bool character_phase)
{
    cptr header = character_selection_header_text(character_phase);

    Term_erase(0, HEADER_ROW, 255);
    Term_putstr(QUESTION_COL, HEADER_ROW, -1, TERM_L_BLUE, header);
}

static int birth_prompt_row(void)
{
    int wid = 80;
    int hgt = 24;
    int row;

    Term_get_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    row = hgt - 1;
    if (row < TABLE_ROW)
        row = TABLE_ROW;

    return row;
}

static int birth_description_base_row(void)
{
    int wid = 80;
    int hgt = 24;
    int row;
    int min_row = TABLE_ROW + A_MAX + 3;
    int max_row = birth_prompt_row() - 1;

    Term_get_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    row = hgt - 5;
    if (row > DESCRIPTION_ROW)
        row = DESCRIPTION_ROW;
    if (row < min_row)
        row = min_row;
    if (row > max_row)
        row = max_row;
    if (row < TABLE_ROW + 1)
        row = TABLE_ROW + 1;

    return row;
}

static int choice_description_row(int visible_rows, bool allow_full_description_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = birth_description_base_row();
    int min_row;

    (void)wid;

    if (allow_full_description_screen)
        return row;

    Term_get_size(&wid, &hgt);
    if (hgt < 1)
        hgt = 24;

    if (hgt > 20)
        return row;

    /* On short screens, use the first free row after the list. */
    min_row = TABLE_ROW + visible_rows;
    row = min_row;

    if (row > birth_prompt_row() - 1)
        row = birth_prompt_row() - 1;
    if (row < min_row)
        row = min_row;

    return row;
}

static int birth_wrap_col(int indent)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;
    if (indent < 0)
        indent = 0;

    if (wid <= indent + 1)
        return indent + 1;

    return wid - 1;
}

static int birth_wrapped_line_count(cptr text, int indent)
{
    if (!text || !text[0])
        return 0;

    return count_wrapped_lines(text, birth_wrap_col(indent), indent);
}

static void birth_put_wrapped_text(byte attr, cptr text, int row, int col)
{
    if (!text || !text[0])
        return;

    text_out_wrap = birth_wrap_col(col);
    text_out_indent = col;
    Term_gotoxy(col, row);
    text_out_to_screen(attr, text);
    text_out_wrap = 0;
    text_out_indent = 0;
}

static void birth_put_str_fit(byte attr, cptr text, int row, int col)
{
    int wid = 80;
    int hgt = 24;
    int max_len;
    char buf[256];

    if (!text || !text[0])
        return;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;
    if (col < 0)
        col = 0;
    if (col >= wid)
        return;

    max_len = wid - col;
    if (max_len < 1)
        return;

    SDL_strlcpy(buf, text, sizeof(buf));
    if ((int)strlen(buf) > max_len)
        buf[max_len] = '\0';

    Term_putstr(col, row, -1, attr, buf);
}

static cptr birth_wrap_line(cptr text, int width, char *buf, size_t buflen)
{
    cptr start;
    cptr end;
    cptr last_space = NULL;
    cptr next;
    size_t copy_len;

    if (!buf || buflen == 0)
        return text;

    buf[0] = '\0';

    if (!text)
        return text;

    while (*text == ' ')
        text++;

    if (!text[0])
        return text;

    if (width < 1)
        width = 1;

    start = text;
    end = start;

    while (*end && *end != '\n' && (end - start) < width)
    {
        if (*end == ' ')
            last_space = end;
        end++;
    }

    if (*end == '\n' || !*end)
    {
        next = end;
    }
    else if (last_space && last_space > start)
    {
        end = last_space;
        next = last_space;
    }
    else
    {
        next = end;
    }

    while (end > start && end[-1] == ' ')
        end--;

    copy_len = (size_t)(end - start);
    if (copy_len >= buflen)
        copy_len = buflen - 1;

    memcpy(buf, start, copy_len);
    buf[copy_len] = '\0';

    while (*next == ' ')
        next++;

    if (*next == '\n')
        next++;

    while (*next == ' ')
        next++;

    return next;
}

static int birth_wrapped_entry_lines(cptr entries[], int entry_n, int width,
    int max_entries)
{
    int total = 0;
    int limit = entry_n;
    char line_buf[64];

    if (max_entries >= 0 && max_entries < limit)
        limit = max_entries;

    for (int i = 0; i < limit; ++i)
    {
        cptr rest = entries[i];

        while (rest && rest[0])
        {
            rest = birth_wrap_line(rest, width, line_buf, sizeof(line_buf));
            total++;
        }
    }

    return total;
}

static int birth_put_wrapped_entries(byte attr, cptr entries[], int entry_n,
    int row, int col, int width, int max_rows, int max_entries)
{
    int used = 0;
    int limit = entry_n;
    char line_buf[64];

    if (width < 1 || max_rows <= 0)
        return 0;

    if (max_entries >= 0 && max_entries < limit)
        limit = max_entries;

    for (int i = 0; i < limit && used < max_rows; ++i)
    {
        cptr rest = entries[i];

        while (rest && rest[0] && used < max_rows)
        {
            rest = birth_wrap_line(rest, width, line_buf, sizeof(line_buf));
            Term_erase(col, row + used, width);
            Term_putstr(col, row + used, -1, attr, line_buf);
            used++;
        }
    }

    return used;
}

static int choice_description_line_count(cptr text)
{
    return birth_wrapped_line_count(text, 2);
}

static int choice_visible_capacity(int num, cptr text,
    bool allow_full_description_screen)
{
    int wid = 80;
    int term_hgt = 24;
    int description_row = birth_description_base_row();
    int hgt = birth_prompt_row() - TABLE_ROW - 1;

    Term_get_size(&wid, &term_hgt);
    (void)wid;
    if (term_hgt < 1)
        term_hgt = 24;
    if (hgt < 0)
        hgt = 0;

    if (allow_full_description_screen)
    {
        bool compact_flags = character_flags_need_compact_layout();
        bool very_short = (term_hgt <= 20);

        if (compact_flags && very_short)
        {
            int max_hgt = (description_row - 2) - TABLE_ROW;

            if (max_hgt < 0)
                max_hgt = 0;
            if (hgt > max_hgt)
                hgt = max_hgt;
        }
    }
    else if (term_hgt <= 20)
    {
        int min_description_rows = (term_hgt <= 18) ? 5 : 4;
        int description_rows = choice_description_line_count(text);
        int max_hgt;

        if (description_rows > min_description_rows)
            min_description_rows = description_rows;

        max_hgt = birth_prompt_row() - TABLE_ROW - 2 - min_description_rows;
        if (max_hgt < 0)
            max_hgt = 0;
        if (hgt > max_hgt)
            hgt = max_hgt;
    }

    if (num < 1)
        return 0;
    if (num < hgt + 1)
        return num;

    return hgt + 1;
}

static int choice_description_fit_row(int row, int visible_rows, cptr text)
{
    int min_row = TABLE_ROW + visible_rows + 1;
    int max_row = birth_prompt_row() - 1;
    int text_rows = choice_description_line_count(text);

    if (text_rows > 0)
    {
        int fit_row = birth_prompt_row() - text_rows;

        if (fit_row < max_row)
            max_row = fit_row;
    }

    if (max_row < TABLE_ROW + 1)
        max_row = TABLE_ROW + 1;
    if (max_row < min_row)
        max_row = min_row;
    if (row > max_row)
        row = max_row;
    if (row < min_row)
        row = min_row;

    return row;
}

static int collect_character_starting_abilities(int character, cptr out[], int out_max)
{
    int count = 0;

    if (character <= 0)
        return 0;

    if (c_info[character].flags_u & UNQ_MIM)
        return 0;

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[character].a_adj[slot][0];
        int ability = c_info[character].a_adj[slot][1];
        cptr name;

        if (stat < 0)
            break;

        if (stat >= S_MAX || ability < 0 || ability >= ABILITIES_MAX)
            continue;

        name = character_ability_names[stat][ability];
        if (!name)
            continue;

        if (out && count < out_max)
            out[count] = name;

        count++;
    }

    return count;
}

static void display_character_description_screen(birth_menu choice)
{
    int wid = 80;
    int hgt = 24;
    int character_idx;
    char full_name[64];
    int name_col = 2;
    int text_top_row = 2;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    if (hgt <= 18)
        text_top_row = 1;

    character_idx = character_choice_index_by_name(choice.name);
    if (character_idx >= 0)
    {
        strnfmt(full_name, sizeof(full_name), "%s%s",
            c_name + c_info[character_idx].name,
            c_name + c_info[character_idx].alt_name);
    }
    else
    {
        strnfmt(full_name, sizeof(full_name), "%s", choice.name ? choice.name : "");
    }

    if ((int)strlen(full_name) < wid)
        name_col = (wid - (int)strlen(full_name)) / 2;
    if (name_col < 2)
        name_col = 2;

    screen_save();
    Term_clear();

    Term_putstr(name_col, 0, -1, TERM_L_BLUE, full_name);

    if (choice.text && choice.text[0] && (hgt > text_top_row))
    {
        int text_row = text_top_row;
        int text_rows = birth_wrapped_line_count(choice.text, 2);
        int available_rows = hgt - text_top_row - 1;

        if ((hgt > 18) && (text_rows < available_rows))
            text_row = text_top_row + ((available_rows - text_rows) / 2);
        if (text_row < text_top_row)
            text_row = text_top_row;
        if (text_row > hgt - 2)
            text_row = hgt - 2;

        birth_put_wrapped_text(TERM_WHITE, choice.text, text_row, 2);
    }

    if (hgt > 0)
    {
        if (steamdeck_controls_active())
        {
            char back_label[16];
            char prompt_buf[48];

            birth_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] return", back_label);
            birth_put_str_fit(TERM_SLATE, prompt_buf, hgt - 1, 2);
        }
        else
        {
            birth_put_str_fit(TERM_SLATE,
                "Press any key to return", hgt - 1, 2);
        }
    }

    Term_fresh();
    ui_menu_click_begin();
    ui_menu_click_add_full_row('\r', hgt - 1);
    (void)inkey();
    ui_menu_click_clear();

    screen_load();
}

/*
 * Generic "get choice from menu" function
 */
static int get_player_choice(birth_menu* choices, int num, int def, int col,
    int wid, void (*hook)(birth_menu), bool allow_full_description_screen)
{
    enum {
        BIRTH_CHOICE_CLICK_BACK = -1,
        BIRTH_CHOICE_CLICK_SELECT = -2,
        BIRTH_CHOICE_CLICK_DETAILS = -3,
        BIRTH_CHOICE_CLICK_RANDOM = -4,
        BIRTH_CHOICE_CLICK_RIGHT_BACK = -5
    };
    int top = 0, next;
    int i, dir;
    char c;
    bool done = false;
    bool show_description;
    bool compact_flags;
    int prompt_row;
    int hgt;
    byte attr;
    char prompt[160];
    int cur = (def) ? def : 0;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    int clear_limit = birth_prompt_row() + 1;
    int last_list_rows_drawn = 0;
    int last_description_row = birth_prompt_row();

    /* Autoselect if able */
    // if (num == 1) done = true;

    /* Clear */
    if (clear_limit < TABLE_ROW)
        clear_limit = TABLE_ROW;
    for (i = TABLE_ROW; i < clear_limit; i++)
    {
        /* Clear */
        Term_erase(col, i, 255/* Term->wid - wid */);
    }

    /* Choose */
    while (true)
    {
        int description_row = birth_description_base_row();
        int list_rows_drawn;
        int visible_capacity = choice_visible_capacity(num, choices[cur].text,
            allow_full_description_screen);

        hgt = visible_capacity - 1;
        if (hgt < 0)
            hgt = 0;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        /* Redraw the list */
        for (i = 0; ((i + top < num) && (i <= hgt)); i++)
        {
            /* Clear */
            Term_erase(col, i + TABLE_ROW, wid);

            /* Display name part */
            if (i == (cur - top))
            {
                /* Highlight the current selection */
                if (choices[i + top].ghost)
                    attr = TERM_BLUE;
                else
                    attr = TERM_L_BLUE;
            }
            else
            {
                if (choices[i + top].ghost)
                    attr = TERM_SLATE;
                else
                    attr = TERM_WHITE;
            }

            /* Display character name */
            char name_part[256];
            if (choices[i + top].ghost)
                strnfmt(name_part, sizeof(name_part), "%c %s", 'X', choices[i + top].name);
            else 
                strnfmt(name_part, sizeof(name_part), "%s", choices[i + top].name);
            
            Term_putstr(col, i + TABLE_ROW, wid, attr, name_part);
            ui_menu_click_add(i + top, col, i + TABLE_ROW, wid);
        }

        list_rows_drawn = i;

        /*
         * When a newly selected entry needs more description space on a short
         * screen, the visible list can shrink by one or more rows. Clear any
         * rows that belonged to the previous, taller list so stale entries do
         * not remain between the list and the description text.
         */
        for (i = list_rows_drawn; i < last_list_rows_drawn; i++)
            Term_erase(0, TABLE_ROW + i, 255);

        if (!allow_full_description_screen)
        {
            description_row = choice_description_row(list_rows_drawn, false);
            description_row = choice_description_fit_row(description_row,
                list_rows_drawn, choices[cur].text);
        }

        {
            int clear_from_row = description_row;

            if (last_description_row < clear_from_row)
                clear_from_row = last_description_row;

            if (allow_full_description_screen)
            {
                bool compact_flags = character_flags_need_compact_layout();
                bool very_short = (Term->hgt > 0) && (Term->hgt <= 20);
                if (compact_flags && very_short)
                    clear_from_row = description_row - 1;

                /*
                 * On compact screens, the previous race-selection phase can
                 * leave wrapped description text a few rows above the compact
                 * character traits area. Clear from the earlier race
                 * description start as well so stale fragments do not remain
                 * when switching phases.
                 */
                if (compact_flags)
                {
                    int compact_min_clear = TABLE_ROW + list_rows_drawn + 1;
                    int race_visible_rows = choice_visible_capacity(
                        z_info->p_max, p_text + p_info[p_ptr->prace].text, false);
                    int race_description_row = choice_description_row(
                        race_visible_rows, false);
                    race_description_row = choice_description_fit_row(
                        race_description_row, race_visible_rows,
                        p_text + p_info[p_ptr->prace].text);
                    if (compact_min_clear < clear_from_row)
                        clear_from_row = compact_min_clear;
                    if (race_description_row < clear_from_row)
                        clear_from_row = race_description_row;
                }
            }
            for (i = clear_from_row; i < Term->hgt; i++)
                Term_erase(0, i, 255);
        }

        compact_flags = (allow_full_description_screen && character_flags_need_compact_layout());
        show_description = (!compact_flags);

        if (allow_full_description_screen)
            show_description = (show_description && character_description_has_room());

        /* Display auxiliary information before the description so the
         * description text remains the topmost layer on short screens. */
        if (hook)
            hook(choices[cur]);

        if (show_description && choices[cur].text != NULL)
        {
            birth_put_wrapped_text(TERM_WHITE, choices[cur].text,
                description_row, 2);
        }

        last_list_rows_drawn = list_rows_drawn;
        last_description_row = description_row;

        if (done)
        {
            ui_menu_click_clear();
            return (cur);
        }

        if (Term->hgt > 0)
        {
            prompt_row = birth_prompt_row();
            Term_erase(0, prompt_row, 255);

            if (steamdeck)
            {
                char confirm_label[16];
                char detail_label[16];
                char back_label[16];
                char random_label[16];

                birth_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_alt_action_key(), "X",
                    detail_label, sizeof(detail_label));
                birth_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                birth_prompt_label('r', "r", random_label,
                    sizeof(random_label));

                if (allow_full_description_screen)
                    strnfmt(prompt, sizeof(prompt),
                        "D-pad move  %s select  %s details  %s back  %s random",
                        confirm_label, detail_label, back_label, random_label);
                else
                    strnfmt(prompt, sizeof(prompt),
                        "D-pad move  %s select  %s back  %s random",
                        confirm_label, back_label, random_label);
            }
            else if (allow_full_description_screen)
            {
                strnfmt(prompt, sizeof(prompt),
                    "SPACE/ENTER select  f description  ESC back  r random");
            }
            else
            {
                strnfmt(prompt, sizeof(prompt),
                    "SPACE/ENTER select  ESC back  r random");
            }
            Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt);
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_SELECT,
                QUESTION_COL, prompt_row, prompt, "select");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_DETAILS,
                QUESTION_COL, prompt_row, prompt, "description");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_DETAILS,
                QUESTION_COL, prompt_row, prompt, "details");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_BACK,
                QUESTION_COL, prompt_row, prompt, "back");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_BACK,
                QUESTION_COL, prompt_row, prompt, "ESC");
            ui_menu_click_add_text_token(BIRTH_CHOICE_CLICK_RANDOM,
                QUESTION_COL, prompt_row, prompt, "random");
        }

        if (allow_full_description_screen)
        {
            int term_wid = 80;
            int term_hgt = 24;

            Term_get_size(&term_wid, &term_hgt);
            if (term_hgt < 1)
                term_hgt = 24;

            (void)term_wid;
            for (i = 0; i < term_hgt; i++)
                ui_menu_click_add_full_row(BIRTH_CHOICE_CLICK_RIGHT_BACK, i);
        }

        /* Move the cursor */
        put_str("", TABLE_ROW + cur - top, col);

        hide_cursor = true;
        c = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (click_action == UI_MENU_CLICK_SECONDARY
                    && allow_full_description_screen)
                {
                    c = ESCAPE;
                    clicked_choice = BIRTH_CHOICE_CLICK_BACK;
                    click_action = UI_MENU_CLICK_PRIMARY;
                }
                else if (clicked_choice == BIRTH_CHOICE_CLICK_RIGHT_BACK)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    continue;
                }
                else if (clicked_choice >= 0 && clicked_choice < num)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != cur)
                    {
                        cur = clicked_choice;
                        if (cur < top || cur > top + hgt)
                            top = cur;
                        continue;
                    }

                    if (choices[cur].ghost)
                        bell("Your race cannot choose that character.");
                    else
                        return (cur);
                    continue;
                }

                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                switch (clicked_choice)
                {
                case BIRTH_CHOICE_CLICK_BACK: c = ESCAPE; break;
                case BIRTH_CHOICE_CLICK_SELECT: c = '\r'; break;
                case BIRTH_CHOICE_CLICK_DETAILS: c = 'f'; break;
                case BIRTH_CHOICE_CLICK_RANDOM: c = 'r'; break;
                case BIRTH_CHOICE_CLICK_RIGHT_BACK: break;
                default: break;
                }
            }
        }

        /* Exit the game */
        if ((c == 'Q') || (c == 'q'))
            quit(NULL);

        /* Hack - go back */
        if ((c == ESCAPE) || (c == '4')
            || (steamdeck && c == steamdeck_back_key()))
        {
            ui_menu_click_clear();
            return (INVALID_CHOICE);
        }

        /* Make a choice */
        if (birth_confirm_input(c, steamdeck) || (c == '6')) {
            if (choices[cur].ghost)
                bell("Your race cannot choose that character.");
            else
            {
                ui_menu_click_clear();
                return (cur);
            }
        }
        // Show scores (short): accept both 's' and 'S'
        if (c == 's' || c == 'S')
        {
            show_scores_interactive(false);
            continue; /* Return to the selection loop after showing scores */
        }
        
        // Show help: accept both 'h' and 'H', plus '?'
        if (c == 'h' || c == 'H' || c == '?')
        {
            do_cmd_help();
            continue; /* Return to the selection loop after showing help */
        }

        if (allow_full_description_screen
            && (c == 'f' || c == 'F'
                || (steamdeck && c == steamdeck_alt_action_key())))
        {
            display_character_description_screen(choices[cur]);
            continue;
        }

        /* Random choice */
        if (c == 'r')
        {
            /* Ensure legal choice */
            do
            {
                cur = rand_int(num);
            } while (choices[cur].ghost);

            /* Done */
            done = true;
        }

        /* Alphabetic choice */
    else if (menu_letters && isalpha((unsigned char)c))
        {
            /* Options */
            if ((c == 'O') || (c == 'o'))
            {
                do_cmd_options();
            }

            else
            {
                int choice;

                if (islower(c))
                    choice = A2I(c);
                else
                    choice = c - 'A' + 26;

        /* Validate input */
        if ((choice > -1) && (choice < num) && !(choices[choice].ghost))
                {
                    cur = choice;

                    /* Done */
                    done = true;
                }
        else if ((choice > -1) && (choice < num) && choices[choice].ghost)
                {
                    bell("Your race cannot choose that character.");
                }
                else
                {
                    bell("Illegal response to question!");
                }
            }
        }

        /* Move */
        else if (isdigit(c))
        {
            /* Get a direction from the key */
            dir = target_dir(c);

            /* Going up? */
            if (dir == 8)
            {
                next = -1;
                for (i = 0; i < cur; i++)
                {
                    // if (!(choices[i].ghost))
                    // {
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
                /* if (cur != 0) cur--; */

                /* Scroll up */
                if ((top > 0) && ((cur - top) < 4))
                    top--;
            }

            /* Going down? */
            if (dir == 2)
            {
                next = -1;
                for (i = num - 1; i > cur; i--)
                {
                    // if (!(choices[i].ghost))
                    //
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
                /* if (cur != (num - 1)) cur++; */

                /* Scroll down */
                if ((top + hgt < (num - 1)) && ((top + hgt - cur) < 4))
                    top++;
            }
        }

        /* Invalid input */
        else
            bell("Illegal response to question!");

        /* If choice is off screen, move it to the top */
        if ((cur < top) || (cur > top + hgt))
            top = cur;
    }

    ui_menu_click_clear();
    return (INVALID_CHOICE);
}

/* OR of every flag carried by the active metarun curses */
u32b curse_flag_mask(void)
{
    u32b m = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_CURSE_STACK(id) > 0) m |= cu_info[id].flags;
    }
    return m;
}

/* Count active curse STACKS that carry an RHF flag (cu_info[].flags) */
int curse_flag_count_rhf(u32b rhf_flag)
{
    int count = 0;
    /* Iterate over every defined curse */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        /* Get the stack count for this curse */
        int stacks = CURSE_GET(i);
        if (stacks > 0) {
            if (cu_info[i].flags & rhf_flag) count += stacks;
        } else if (stacks < 0) {
            if (cu_info[i].blessing_flags & rhf_flag) count += -stacks;
        }
    }
    return count;
}

/* Count active curse STACKS that carry a CUR flag (cu_info[].flags_u) */
int curse_flag_count_cur(u32b cur_flag)
{
    int count = 0;

    /* Iterate over every defined curse */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        /* Get the stack count for this curse */
        int stacks = CURSE_GET(i);
        if (stacks > 0) {
            if (cu_info[i].flags_u & cur_flag) count += stacks;
        } else if (stacks < 0) {
            if (cu_info[i].blessing_flags_u & cur_flag) count += -stacks;
        }
    }

    return count;
}

/* Signed delta for CUR flags: positive for curses, negative for blessings */
int curse_flag_delta_cur(u32b cur_flag)
{
    int delta = 0;

    for (int i = 0; i < z_info->cu_max; i++)
    {
        int stacks = CURSE_GET(i);
        if (stacks > 0) {
            if (cu_info[i].flags_u & cur_flag) delta += stacks;
        } else if (stacks < 0) {
            if (cu_info[i].blessing_flags_u & cur_flag) delta -= (-stacks);
        }
    }

    return delta;
}

typedef struct {
    cptr txt;
    byte attr;
} birth_compact_flag_line;

static int collect_character_trait_lines(int race, int character, bool short_labels,
    birth_compact_flag_line out[], int out_max, int* max_line_len)
{
    int total = 0;

    byte attr_affinity = TERM_GREEN;
    byte attr_mastery = TERM_L_GREEN;
    byte attr_penalty = TERM_RED;
    byte attr_gr_penalty = TERM_L_RED;

    birth_compact_flag_line uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color)                                             \
    do {                                                                      \
        if ((text) && (n) < (int)N_ELEMENTS(arr))                             \
        {                                                                     \
            (arr)[(n)].txt = (text);                                          \
            (arr)[(n)++].attr = (color);                                      \
        }                                                                     \
    } while (0)

#define HANDLE_SKILL_EX(LABEL_LONG, LABEL_SHORT, AFF_FLAG, PEN_FLAG)          \
    do {                                                                      \
        int score = 0;                                                        \
        if (p_info[race].flags & (AFF_FLAG)) score++;                         \
        if (c_info[character].flags & (AFF_FLAG)) score++;                    \
        if ((PEN_FLAG) && (p_info[race].flags & (PEN_FLAG))) score--;         \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) score--;    \
        score += curse_flag_count_rhf(AFF_FLAG);                              \
        if ((PEN_FLAG)) score -= curse_flag_count_rhf(PEN_FLAG);              \
        if (score > 2) score = 2;                                             \
        if (score < -2) score = -2;                                           \
        if (score == 2)                                                       \
            PUSH(ma_buf, ma_n,                                                \
                short_labels ? LABEL_SHORT "++" : LABEL_LONG " mastery",      \
                attr_mastery);                                                \
        else if (score == 1)                                                  \
            PUSH(af_buf, af_n,                                                \
                short_labels ? LABEL_SHORT "+ " : LABEL_LONG " affinity",     \
                attr_affinity);                                               \
        else if (score == -1)                                                 \
            PUSH(pen_buf, pen_n,                                              \
                short_labels ? LABEL_SHORT "- " : LABEL_LONG " penalty",      \
                attr_penalty);                                                \
        else if (score == -2)                                                 \
            PUSH(pen_buf, pen_n,                                              \
                short_labels ? LABEL_SHORT "--" : LABEL_LONG " grand penalty",\
                attr_gr_penalty);                                             \
    } while (0)

#define HANDLE_UNIQUE_EX(LABEL_LONG, LABEL_SHORT, FLAG, COLOR)                \
    do {                                                                      \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG))) \
            PUSH(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG, (COLOR)); \
    } while (0)

#define HANDLE_UNIQUE_U_EX(LABEL_LONG, LABEL_SHORT, FLAG, COLOR)              \
    do {                                                                      \
        if (c_info[character].flags_u & (FLAG))                               \
            PUSH(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG, (COLOR)); \
    } while (0)

#define EMIT(arr, n)                                                          \
    do {                                                                      \
        for (int _i = 0; _i < (n); ++_i)                                      \
        {                                                                     \
            cptr _txt = (arr)[_i].txt ? (arr)[_i].txt : "";                  \
            if (max_line_len && (int)strlen(_txt) > *max_line_len)            \
                *max_line_len = (int)strlen(_txt);                            \
            if (out && total < out_max)                                       \
                out[total] = (arr)[_i];                                       \
            total++;                                                          \
        }                                                                     \
    } while (0)

    HANDLE_SKILL_EX("melee", "melee", RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion", "evasion", RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth", "stealth", RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery", "archery", RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will", "will", RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", "perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing", "smithing", RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song", "song", RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow", "bow", RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe", "axe", RHF_AXE_PROFICIENCY, 0);

    HANDLE_UNIQUE_U_EX("Master Artisan", "Master Artisan", UNQ_SMT_FEANOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Galvorn", "Galvorn Maker", UNQ_SMT_EOL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("One Handed", "One Handed", UNQ_MEL_MAEDHROS, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Agarwaen", "Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Hidden city", "Hidden City", UNQ_SNG_TURGON, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Chosen of Ulmo", "Ulmo's Chosen", UNQ_WIL_TUOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Indominable Will", "Indom. Will", UNQ_EARENDIL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Orome Himself", "Orome", UNQ_WIL_FIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Songs of Power", "Songs of Power", UNQ_SNG_FIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Elven Dance", "Elven Dance", UNQ_SNG_LUT, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Girdle of Melian", "Melian's Girdle", UNQ_SNG_MEL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Angrist", "Angrist Maker", UNQ_SMT_TELCHAR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Old Master", "Old Master", UNQ_SMT_GAMIL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Ring Master", "Ring Master", UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Aure entuluva", "Aure Entuluva", UNQ_SNG_HURIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Voice of Girdle", "Girdle Voice", UNQ_SNG_THINGOL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Forgotten", "Forgotten", UNQ_MIM, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Minstrel", "Minstrel", UNQ_MINSTREL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Woven Master", "Woven Master", UNQ_WOVEN_MASTER, TERM_VIOLET);

    HANDLE_UNIQUE_EX("Gift of Eru", "Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Seafarer", "Seafarer", RHF_FREE, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Kinslayer", "Kinslayer", RHF_KINSLAYER, TERM_UMBER);
    HANDLE_UNIQUE_EX("Treacherous", "Treacherous", RHF_TREACHERY, TERM_UMBER);
    HANDLE_UNIQUE_EX("Doom of Mandos", "Mandos' Doom", RHF_CURSE, TERM_UMBER);
    HANDLE_UNIQUE_EX("Morgoth Curse", "Morgoth Curse", RHF_MOR_CURSE, TERM_UMBER);

    EMIT(uniq_buf, uniq_n);
    EMIT(ma_buf, ma_n);
    EMIT(af_buf, af_n);
    EMIT(pen_buf, pen_n);

#undef EMIT
#undef HANDLE_UNIQUE_U_EX
#undef HANDLE_UNIQUE_EX
#undef HANDLE_SKILL_EX
#undef PUSH

    return total;
}


/*
 * Show race/character flags in priority order.
 * Masteries first, then single-side affinities, then penalties,
 * and finally any "headline / unique" flags.
 */
static void print_rh_flags(int race, int character, int col, int row)
{
    int flags_left  = 0;
    int flags_right = 0;
    bool compact_layout = character_flags_need_compact_layout();
    int description_row = birth_description_base_row();
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    cptr ability_lines[CHARACTER_ABILITY_MAX];
    int ability_line_n = collect_character_starting_abilities(character,
        ability_lines, N_ELEMENTS(ability_lines));

    byte attr_affinity = TERM_GREEN;
    byte attr_mastery  = TERM_L_GREEN;
    byte attr_penalty  = TERM_RED;
    byte attr_gr_penalty  = TERM_L_RED;

    const int col_pen = col + 21;

    // Updated struct to support side
    typedef struct {
        const char *txt;
        byte col;
        int side;  // 0 = left, 1 = right
    } skill_line;

    skill_line mastery_buf [16], affinity_buf[16], penalty_buf[16], unique_buf[32];
    int mastery_n = 0, affinity_n = 0, penalty_n = 0, unique_n = 0;

/*
 * Show one skill line according to the new +/-2<->mastery / grand-penalty rule.
 *
 *   +1 for every ..._AFFINITY bit, -1 for every ..._PENALTY bit.
 *
 *        score   meaning            colour / buffer
 *        =====   ===============    =========================
 *          +2    mastery            mastery_buf  / attr_mastery
 *          +1    affinity           affinity_buf / attr_affinity
 *           0    (omit line)        -
 *          -1    penalty            penalty_buf  / attr_penalty
 *          -2    grand penalty      penalty_buf  / attr_penalty
 */
/* Show one skill line according to the new +/-2 rule,
 * now counting curse affinities / penalties too.
 */
#define HANDLE_SKILL_EX(label, AFF_FLAG, PEN_FLAG)                          \
    do {                                                                    \
        int score = 0;                                                      \
                                                                            \
        /* race + character bits */                                             \
        if (p_info[race].flags  & (AFF_FLAG)) score++;                      \
        if (c_info[character].flags & (AFF_FLAG)) score++;                      \
        if (p_info[race].flags  & (PEN_FLAG)) score--;                      \
        if (c_info[character].flags & (PEN_FLAG)) score--;                      \
                                                                            \
        /* every copy of the same *RHF* curse flag */                       \
        score += curse_flag_count_rhf(AFF_FLAG);                            \
        score -= curse_flag_count_rhf(PEN_FLAG);                            \
                                                                            \
        /* clamp so the UI never shows >mastery or >grand-penalty */        \
        if (score >  2) score =  2;                                         \
        if (score < -2) score = -2;                                         \
                                                                            \
        if (score ==  2) {                                                  \
            mastery_buf[mastery_n].txt = label " mastery";                  \
            mastery_buf[mastery_n++].col = attr_mastery;                    \
        } else if (score == 1) {                                            \
            affinity_buf[affinity_n].txt = label " affinity";               \
            affinity_buf[affinity_n++].col = attr_affinity;                 \
        } else if (score == -1) {                                           \
            penalty_buf[penalty_n].txt = label " penalty";                  \
            penalty_buf[penalty_n++].col = attr_penalty;                    \
        } else if (score == -2) {                                           \
            penalty_buf[penalty_n].txt = label " grand penalty";            \
            penalty_buf[penalty_n++].col = attr_gr_penalty;                 \
        }                                                                   \
    } while (0)


// New: (label, FLAG, COLOR, SIDE) where SIDE = 0 (left) or 1 (right)
#define HANDLE_UNIQUE(label, FLAG, COLOR, SIDE)                             \
    do {                                                                    \
        int race_has     = p_info[race].flags & (FLAG);                     \
        int character_has = c_info[character].flags & (FLAG);               \
        if (race_has || character_has) {                                    \
            unique_buf[unique_n].txt  = label;                              \
            unique_buf[unique_n].col  = (COLOR);                            \
            unique_buf[unique_n++].side = (SIDE);                           \
        }                                                                   \
    } while (0)

// New: (label, FLAG, COLOR, SIDE) where SIDE = 0 (left) or 1 (right)
#define HANDLE_UNIQUE_U(label, FLAG, COLOR, SIDE)                             \
    do {                                                                    \
        int character_has = c_info[character].flags_u & (FLAG);             \
        if (character_has) {                                                \
            unique_buf[unique_n].txt  = label;                              \
            unique_buf[unique_n].col  = (COLOR);                            \
            unique_buf[unique_n++].side = (SIDE);                           \
        }                                                                   \
    } while (0)

    // Skills
    HANDLE_SKILL_EX("melee",      RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion",    RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth",    RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery",    RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will",       RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing",   RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song",       RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow",        RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe",        RHF_AXE_PROFICIENCY, 0);

    // Unique skills: SIDE = 0 (left), 1 (right)
    HANDLE_UNIQUE_U("Master Artisan",   UNQ_SMT_FEANOR,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Creator of Galvorn",   UNQ_SMT_EOL,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("One Handed",   UNQ_MEL_MAEDHROS,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Agarwaen",   UNQ_WIL_TURIN,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Hidden city",   UNQ_SNG_TURGON,     TERM_VIOLET,     1);
    HANDLE_UNIQUE_U("Chosen of Ulmo",   UNQ_WIL_TUOR, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Indominable Will",   UNQ_EARENDIL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Orome Himself",   UNQ_WIL_FIN, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Songs of Power",   UNQ_SNG_FIN, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Elven Dance",   UNQ_SNG_LUT, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Girdle of Melian",   UNQ_SNG_MEL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Creator of Angrist",   UNQ_SMT_TELCHAR, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Old Master",   UNQ_SMT_GAMIL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Ring Master",   UNQ_SMT_CELEBRIMBOR, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Aure entuluva",   UNQ_SNG_HURIN, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Voice of Girdle",   UNQ_SNG_THINGOL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Forgotten",   UNQ_MIM, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Minstrel",   UNQ_MINSTREL, TERM_VIOLET,   1);
    HANDLE_UNIQUE_U("Woven Master",   UNQ_WOVEN_MASTER, TERM_VIOLET,   1);

    HANDLE_UNIQUE("Gift of Eru",   RHF_GIFTERU,     TERM_VIOLET,     1);
    HANDLE_UNIQUE("Seafarer",   RHF_FREE, TERM_VIOLET,   1);

    HANDLE_UNIQUE("Kinslayer",   RHF_KINSLAYER, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Treacherous",   RHF_TREACHERY, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Doom of Mandos",   RHF_CURSE, TERM_UMBER,   1); // right
    HANDLE_UNIQUE("Morgoth Curse",   RHF_MOR_CURSE, TERM_UMBER,   1); // right

    if (compact_layout)
    {
        char line_buf[64];
        birth_compact_flag_line compact_lines[64];
        birth_compact_flag_line short_trait_lines[64];
        int compact_line_n = 0;
        int compact_max_line_len = 0;
        int short_trait_n = 0;
        int short_trait_max_line_len = 0;
        int prompt_row = birth_prompt_row();
        bool tight_height = character_selection_tight_height();
        bool use_swapped_layout = false;

        compact_line_n = collect_character_trait_lines(race, character, false,
            compact_lines, N_ELEMENTS(compact_lines), &compact_max_line_len);
        short_trait_n = collect_character_trait_lines(race, character, true,
            short_trait_lines, N_ELEMENTS(short_trait_lines), &short_trait_max_line_len);

        if (tight_height)
        {
            const int target_traits = (short_trait_n < 9) ? short_trait_n : 9;
            const int min_upper_two_col_wid = 12;
            int upper_rows_total = description_row - row;
            int lower_rows_total = prompt_row - description_row;
            int upper_col = col;
            int upper_width = term_wid - upper_col;
            int upper_col_gap = 2;
            int upper_col_wid = (upper_width - upper_col_gap) / 2;
            int lower_width = term_wid - 2;
            int target_ability_rows;
            bool show_trait_heading = true;
            bool show_ability_heading = true;
            bool use_upper_two_columns = false;
            bool traits_fit = false;
            bool abilities_fit = false;

            if (upper_rows_total < 0)
                upper_rows_total = 0;
            if (lower_rows_total < 0)
                lower_rows_total = 0;
            if (upper_col_wid < 0)
                upper_col_wid = 0;
            if (lower_width < 1)
                lower_width = 1;

            target_ability_rows = birth_wrapped_entry_lines(ability_lines,
                ability_line_n, lower_width, 3);

            if (target_ability_rows == 0)
            {
                show_ability_heading = false;
                abilities_fit = true;
            }
            else if ((lower_rows_total - 1) >= target_ability_rows)
            {
                show_ability_heading = true;
                abilities_fit = true;
            }
            else if (lower_rows_total >= target_ability_rows)
            {
                show_ability_heading = false;
                abilities_fit = true;
            }

            if (target_traits == 0)
            {
                show_trait_heading = false;
                traits_fit = true;
            }
            else if (upper_col_wid >= min_upper_two_col_wid)
            {
                if ((upper_rows_total - 1) > 0 && ((upper_rows_total - 1) * 2 >= target_traits))
                {
                    show_trait_heading = true;
                    use_upper_two_columns = true;
                    traits_fit = true;
                }
                else if (upper_rows_total > 0 && (upper_rows_total * 2 >= target_traits))
                {
                    show_trait_heading = false;
                    use_upper_two_columns = true;
                    traits_fit = true;
                }
            }

            if (!traits_fit && upper_width >= short_trait_max_line_len)
            {
                if ((upper_rows_total - 1) >= target_traits)
                {
                    show_trait_heading = true;
                    use_upper_two_columns = false;
                    traits_fit = true;
                }
                else if (upper_rows_total >= target_traits)
                {
                    show_trait_heading = false;
                    use_upper_two_columns = false;
                    traits_fit = true;
                }
            }

            use_swapped_layout = traits_fit && abilities_fit;

            if (use_swapped_layout)
            {
                int traits_row = row;
                int ability_row = description_row;

                if (show_trait_heading && short_trait_n > 0)
                    Term_putstr(upper_col, traits_row++, -1, TERM_L_BLUE, "Traits:");

                if (use_upper_two_columns)
                {
                    int col2 = upper_col + upper_col_wid + upper_col_gap;
                    int rows_per_col = description_row - traits_row;
                    int draw_lines;
                    int left_count;
                    int right_count;

                    if (rows_per_col < 0)
                        rows_per_col = 0;
                    draw_lines = (short_trait_n < rows_per_col * 2)
                        ? short_trait_n : rows_per_col * 2;
                    left_count = (draw_lines + 1) / 2;
                    if (left_count > rows_per_col)
                        left_count = rows_per_col;
                    right_count = draw_lines - left_count;
                    if (right_count > rows_per_col)
                        right_count = rows_per_col;
                    left_count = draw_lines - right_count;

                    for (int i = 0; i < left_count; ++i)
                    {
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", upper_col_wid,
                            upper_col_wid, short_trait_lines[i].txt);
                        Term_putstr(upper_col, traits_row + i, -1,
                            short_trait_lines[i].attr, line_buf);
                    }

                    for (int i = 0; i < right_count; ++i)
                    {
                        int idx = left_count + i;
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", upper_col_wid,
                            upper_col_wid, short_trait_lines[idx].txt);
                        Term_putstr(col2, traits_row + i, -1,
                            short_trait_lines[idx].attr, line_buf);
                    }
                }
                else
                {
                    int rows = description_row - traits_row;
                    int draw_lines;

                    if (rows < 0)
                        rows = 0;
                    draw_lines = (short_trait_n < rows) ? short_trait_n : rows;

                    for (int i = 0; i < draw_lines; ++i)
                        Term_putstr(upper_col, traits_row + i, -1,
                            short_trait_lines[i].attr, short_trait_lines[i].txt);
                }

                if (show_ability_heading && ability_line_n > 0)
                    Term_putstr(2, ability_row++, -1, TERM_L_BLUE, "Abilities:");

                {
                    int rows = prompt_row - ability_row;
                    int ability_width = term_wid - 2;

                    if (rows < 0)
                        rows = 0;
                    if (ability_width < 1)
                        ability_width = 1;

                    birth_put_wrapped_entries(TERM_YELLOW, ability_lines,
                        ability_line_n, ability_row, 2, ability_width, rows,
                        ability_line_n);
                }
            }
        }

        if (!use_swapped_layout)
        {
            int compact_row = description_row;
            int compact_col = 2;
            int col_gap = 2;
            int col_wid;
            int ability_width = term_wid - col;
            int ability_rows = description_row - row;
            bool use_two_columns = false;
            int right_offset = 0; /* 0=normal, -1=right column starts on title row, -2=one row above */

            if (ability_width < 1)
                ability_width = 1;
            if (ability_rows < 0)
                ability_rows = 0;

            birth_put_wrapped_entries(TERM_YELLOW, ability_lines, ability_line_n,
                row, col, ability_width, ability_rows, ability_line_n);

            col_wid = (term_wid - compact_col - col_gap) / 2;
            if (col_wid < 1)
                col_wid = 1;

            if (col_wid >= compact_max_line_len)
                use_two_columns = true;

            {
                const bool short_screen = (Term->hgt > 0) && (Term->hgt < 24);
                const int target_limit = tight_height ? 9 : 10;
                const int target_traits = (compact_line_n < target_limit) ? compact_line_n : target_limit;
                const int min_col_wid_for_forced_two_cols = 14;

#define MAX0(v) ((v) > 0 ? (v) : 0)
#define CAPACITY_ONE(_row) (MAX0(Term->hgt - ((_row) + 1) - 1))
#define CAPACITY_TWO(_row, _roff) \
    (MAX0(Term->hgt - ((_row) + 1) - 1) + MAX0(Term->hgt - ((_row) + 1 + (_roff)) - 1))

                int base_capacity = use_two_columns ? CAPACITY_TWO(compact_row, right_offset)
                                                   : CAPACITY_ONE(compact_row);

                if (short_screen && (base_capacity < target_traits))
                {
                    if (compact_row > 0)
                        compact_row = description_row - 1;

                    base_capacity = use_two_columns ? CAPACITY_TWO(compact_row, right_offset)
                                                   : CAPACITY_ONE(compact_row);

                    if ((base_capacity < target_traits) && !use_two_columns
                        && (col_wid >= min_col_wid_for_forced_two_cols))
                    {
                        use_two_columns = true;
                        base_capacity = CAPACITY_TWO(compact_row, right_offset);
                    }

                    if ((base_capacity < target_traits) && use_two_columns)
                    {
                        right_offset = -1;
                        base_capacity = CAPACITY_TWO(compact_row, right_offset);
                    }

                    if ((base_capacity < target_traits) && use_two_columns && (compact_row > 0))
                    {
                        right_offset = -2;
                        base_capacity = CAPACITY_TWO(compact_row, right_offset);
                    }
                }

#undef CAPACITY_TWO
#undef CAPACITY_ONE
#undef MAX0
            }

            {
                int compact_available = Term->hgt - compact_row - 1;

                if ((compact_available > 0) && (Term->hgt > 0))
                    Term_putstr(compact_col, compact_row, -1, TERM_L_BLUE, "Character traits:");

                if (use_two_columns)
                {
                    int col2 = compact_col + col_wid + col_gap;
                    int left_start = compact_row + 1;
                    int right_start = compact_row + 1 + right_offset;
                    int left_rows = Term->hgt - left_start - 1;
                    int right_rows = Term->hgt - right_start - 1;
                    int max_lines;
                    int draw_lines;
                    int left_count;
                    int right_count;

                    if (left_rows < 0) left_rows = 0;
                    if (right_rows < 0) right_rows = 0;

                    max_lines = left_rows + right_rows;
                    draw_lines = (compact_line_n < max_lines) ? compact_line_n : max_lines;

                    left_count = (draw_lines + 1) / 2;
                    if (left_count > left_rows) left_count = left_rows;
                    right_count = draw_lines - left_count;
                    if (right_count > right_rows) right_count = right_rows;
                    if (left_count > (draw_lines - right_count))
                        left_count = draw_lines - right_count;
                    draw_lines = left_count + right_count;

                    for (int i = 0; i < left_count; ++i)
                    {
                        int y = left_start + i;
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", col_wid, col_wid,
                            compact_lines[i].txt);
                        Term_putstr(compact_col, y, -1, compact_lines[i].attr, line_buf);
                    }

                    for (int i = 0; i < right_count; ++i)
                    {
                        int idx = left_count + i;
                        int y = right_start + i;
                        strnfmt(line_buf, sizeof(line_buf), "%-*.*s", col_wid, col_wid,
                            compact_lines[idx].txt);
                        Term_putstr(col2, y, -1, compact_lines[idx].attr, line_buf);
                    }
                }
                else
                {
                    int start_row = compact_row + 1;
                    int rows = Term->hgt - start_row - 1;
                    int draw_lines;

                    if (rows < 0)
                        rows = 0;
                    draw_lines = (compact_line_n < rows) ? compact_line_n : rows;

                    for (int i = 0; i < draw_lines; ++i)
                        Term_putstr(compact_col, start_row + i, -1,
                            compact_lines[i].attr, compact_lines[i].txt);
                }
            }
        }
    }
    else
    {
        // Left column
        for (int i = 0; i < unique_n; ++i)
            if (unique_buf[i].side == 0)
                Term_putstr(col, row + flags_left++, -1, unique_buf[i].col, unique_buf[i].txt);
        for (int i = 0; i < mastery_n;  ++i)
            Term_putstr(col, row + flags_left++, -1, mastery_buf[i].col, mastery_buf[i].txt);
        for (int i = 0; i < affinity_n; ++i)
            Term_putstr(col, row + flags_left++, -1, affinity_buf[i].col, affinity_buf[i].txt);

        // Right column
        for (int i = 0; i < unique_n; ++i)
            if (unique_buf[i].side == 1)
                Term_putstr(col_pen, row + flags_right++, -1, unique_buf[i].col, unique_buf[i].txt);
        for (int i = 0; i < penalty_n; ++i)
            Term_putstr(col_pen, row + flags_right++, -1, penalty_buf[i].col, penalty_buf[i].txt);
    }

#undef HANDLE_SKILL_EX
#undef HANDLE_UNIQUE
#undef HANDLE_UNIQUE_U

if (!compact_layout)
{
    Term_erase(col +7, row - 5, 30);


/* Display starting abilities */
    if (ability_line_n > 0)
    {
        const int x     = col + 7;
        const int y0    = row - 5;
        const int width = 30;   /* how many cols to clear */

        /* 1) clear out every possible line first */
        for (int i = 0; i < CHARACTER_ABILITY_MAX - 3; i++)
        {
            Term_erase(x, y0 + i, width);
        }

        /* 2) now draw the actual list */
        int y = y0;
        int max_lines = CHARACTER_ABILITY_MAX - 3;
        if (ability_line_n < max_lines)
            max_lines = ability_line_n;

        for (int slot = 0; slot < max_lines; slot++)
            Term_putstr(x, y++, -1, TERM_YELLOW, ability_lines[slot]);
    }
}
}

/*
 * Display additional information about each race during the selection.
 */
static void race_aux_hook(birth_menu r_str)
{
    int race, i, adj;
    char s[50];
    byte attr;

    /* Extract the proper race index from the string. */
    for (race = 0; race < z_info->p_max; race++)
    {
        if (!strcmp(r_str.name, p_name + p_info[race].name))
            break;
    }

    if (race == z_info->p_max)
        return;

    /* Display the stats */
    for (i = 0; i < A_MAX; i++)
    {
        /*dump the stats*/
        strnfmt(s, sizeof(s), "%s", stat_names[i]);
        Term_putstr(RACE_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);

        adj = p_info[race].r_adj[i];
        strnfmt(s, sizeof(s), "%+d", adj);

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 0)
            attr = TERM_L_DARK;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else
            attr = TERM_L_BLUE;

        Term_putstr(RACE_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
    }

    /* Display the race flags */

    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE,
        "                         ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE,
        "                         ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE,
        "                         ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE,
        "                        ");

    /* Clear the TOTAL_AUX_COL area (where character info was displayed) */
    Term_putstr(TOTAL_AUX_COL, HEADER_ROW, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 5, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 6, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 7, -1, TERM_WHITE,
        "                                         ");

    // print_rh_flags(race, 0, RACE_AUX_COL, TABLE_ROW + A_MAX + 1);
}

/*
 * Player race
 */
static bool get_player_race(void)
{
    int i;
    birth_menu* races;
    int race;

    races = mem_alloc_array(z_info->p_max, birth_menu);

    /* Tabulate races */
    for (i = 0; i < z_info->p_max; i++)
    {
        races[i].name = p_name + p_info[i].name;
        races[i].ghost = false;
        races[i].text = p_text + p_info[i].text;
    }

    race = get_player_choice(
        races, z_info->p_max, p_ptr->prace, RACE_COL, 15, race_aux_hook, false);

    /* No selection? */
    if (race == INVALID_CHOICE)
    {
        return (false);
    }

    // if different race to last time, then wipe the history, age, height,
    // weight
    if (race != p_ptr->prace)
    {
        p_ptr->history[0] = '\0';
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }
    p_ptr->prace = race;

    /* Save the race pointer */
    rp_ptr = &p_info[p_ptr->prace];

    races = mem_free(races);

    /* Success */
    return (true);
}

// Check character flags
static int is_set(int bit) {
    if (bit < 0 || bit >= FLAG_COUNT) return 0;  // Out of bounds
    int word = bit / 32;
    int shift = bit % 32;
    return (rp_ptr->choice[word] & (1U << shift)) != 0;
}

/*
 * Display additional information about each character during the selection.
 */

static void character_aux_hook(birth_menu c_str)
{
    int character_idx, i, adj;
    int term_wid = 80;
    int term_hgt = 24;
    int description_row = birth_description_base_row();
    bool compact_layout = character_flags_need_compact_layout();
    bool tight_height = character_selection_tight_height();
    int name_col;
    int fallback_name_col;
    bool aligned_name_fits;
    char s[128];
    byte attr;

    /* Extract the proper character index from the string. */
    for (character_idx = 0; character_idx < z_info->c_max; character_idx++)
    {
        if (!strcmp(c_str.name, c_name + c_info[character_idx].name))
            break;
    }

    if (character_idx == z_info->c_max)
        return;

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    /* Clear the entire TOTAL_AUX_COL area FIRST before displaying new info */
    /* Clear from HEADER_ROW down but stop before DESCRIPTION_ROW to preserve history */
    for (i = HEADER_ROW; i < description_row; i++)
    {
        Term_putstr(TOTAL_AUX_COL, i, -1, TERM_WHITE,
            "                                         ");
        /* Also clear the right side area where penalties/flags appear */
        Term_putstr(TOTAL_AUX_COL + 21, i, -1, TERM_WHITE,
            "                                         ");
    }

    /* Also clear the abilities area (col + 7) but only in the same range */
    for (i = 0; i < description_row; i++)
    {
        Term_erase(TOTAL_AUX_COL + 7, i, 60);  /* Wider clearing */
    }

    /* Now display the new stats */
    for (i = 0; i < A_MAX; i++)
    {
        /*dump potential total stats*/
        strnfmt(s, sizeof(s), "%s", stat_names[i]);
        Term_putstr(TOTAL_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);

        adj = c_info[character_idx].h_adj[i] + rp_ptr->r_adj[i] + curses_stat_adj(i);
        strnfmt(s, sizeof(s), "%+d", adj);

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 0)
            attr = TERM_L_DARK;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else
            attr = TERM_L_BLUE;

        Term_putstr(TOTAL_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
    }
    // Check dead
    // if (c_str.ghost) Term_putstr(TOTAL_AUX_COL, QUESTION_ROW + A_MAX + 7, -1, TERM_RED,
    //     "Dead");
    // else Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX +7, -1, TERM_L_BLUE,
    //     "Alive");
    char pretty_name[40];
    strnfmt(pretty_name, sizeof(pretty_name), "%s%s", c_name + c_info[character_idx].name, c_name + c_info[character_idx].alt_name); 
    
    /* Add power stars to the character name */
    char power_stars[16];
    byte star_attr;
    byte power = c_info[character_idx].power;
    switch (power)
    {
        case 0: 
            star_attr = TERM_RED; 
            strnfmt(power_stars, sizeof(power_stars), " *"); 
            break;           /* Weak - 1 red star */
        case 1: 
            star_attr = TERM_WHITE; 
            strnfmt(power_stars, sizeof(power_stars), " **"); 
            break;          /* Average - 2 white stars */
        case 2: 
            star_attr = TERM_GREEN; 
            strnfmt(power_stars, sizeof(power_stars), " ***"); 
            break;         /* Powerful - 3 green stars */
        case 3: 
        case 4:
            star_attr = TERM_L_GREEN; 
            strnfmt(power_stars, sizeof(power_stars), " ***"); 
            break;        /* Very Powerful - 3 bright green stars (P:3 or P:4) */
        default: 
            star_attr = TERM_WHITE; 
            strnfmt(power_stars, sizeof(power_stars), " **"); 
            break;         /* Default to average */
    }
    
    fallback_name_col = QUESTION_COL + (int)strlen(character_selection_header_text(true)) + 1;
    if (fallback_name_col < 0)
        fallback_name_col = 0;
    Term_erase(fallback_name_col, HEADER_ROW, 255);

    aligned_name_fits =
        (TOTAL_AUX_COL + (int)strlen(pretty_name) + (int)strlen(power_stars) < term_wid);
    name_col = aligned_name_fits ? TOTAL_AUX_COL : fallback_name_col;

    Term_putstr(name_col, HEADER_ROW, -1, TERM_L_BLUE, pretty_name);
    Term_putstr(name_col + strlen(pretty_name), HEADER_ROW, -1, star_attr, power_stars);
    
    {
        int legend_row = (compact_layout && tight_height) ? 9 : 10;
        int left_block_width = CLASS_COL;
        int character_list_count = 0;
        int character_list_rows;
        int wide_clear_row;

        if (legend_row < TABLE_ROW + A_MAX + 3)
            legend_row = TABLE_ROW + A_MAX + 3;

        if (left_block_width < 1)
            left_block_width = 1;

        for (i = 0; i < z_info->c_max; i++)
            if (is_set(i))
                character_list_count++;

        character_list_rows = choice_visible_capacity(character_list_count,
            c_str.text, true);
        wide_clear_row = TABLE_ROW + character_list_rows;
        if (wide_clear_row < legend_row)
            wide_clear_row = legend_row;

        for (i = legend_row; i < birth_prompt_row(); ++i)
            Term_erase(0, i, left_block_width);
        for (i = wide_clear_row; i < birth_prompt_row(); ++i)
            Term_erase(0, i, TOTAL_AUX_COL - 1);
    }

    print_rh_flags(
        p_ptr->prace, character_idx, TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1);

    {
        int legend_col = 2;  /* Left side */
        int legend_row = (compact_layout && tight_height) ? 9 : 10;
        int legend_limit_row = birth_prompt_row();
        bool legend_has_room;

        legend_has_room = (legend_row + 3 < legend_limit_row);
        if (compact_layout)
        {
            int compact_first_row = description_row;

            /*
             * Compact flags can start one row above description_row when the
             * screen is short. Keep the optional power legend whenever its
             * rows end before the compact block starts; no blank separator is
             * required.
             */
            if (Term && Term->hgt > 0 && Term->hgt < 24)
                compact_first_row--;

            legend_has_room = !tight_height
                && (compact_first_row > legend_row + 3)
                && (legend_row + 3 < legend_limit_row);
        }

        if (legend_has_room)
        {
            /* Count alive heroes by power level across ALL races */
            int power_counts[4] = {0, 0, 0, 0};  /* weak, fair, strong, mighty (P:3/P:4) */
            for (int i = 0; i < z_info->c_max; i++)
            {
                /* Count only characters that are NOT dead (alive) */
                if (highscore_dead(c_name + c_info[i].name) == 0)  /* If NOT dead (alive) */
                {
                    byte power = c_info[i].power;
                    if (power <= 4)
                    {
                        if (power == 4)
                            power_counts[3]++;  /* P:4 counts toward "Mighty" (same group as P:3) */
                        else
                            power_counts[power]++;
                    }
                }
            }

            /* Display legend without "Power Rating:" header */
            Term_putstr(legend_col, legend_row, -1, TERM_L_GREEN, "***");
            strnfmt(s, sizeof(s), "Mighty %d", power_counts[3]);
            Term_putstr(legend_col + 4, legend_row, -1, TERM_WHITE, s);

            Term_putstr(legend_col, legend_row + 1, -1, TERM_GREEN, "***");
            strnfmt(s, sizeof(s), "Strong %d", power_counts[2]);
            Term_putstr(legend_col + 4, legend_row + 1, -1, TERM_WHITE, s);

            Term_putstr(legend_col, legend_row + 2, -1, TERM_WHITE, "**");
            strnfmt(s, sizeof(s), "Fair %d", power_counts[1]);
            Term_putstr(legend_col + 4, legend_row + 2, -1, TERM_WHITE, s);

            Term_putstr(legend_col, legend_row + 3, -1, TERM_RED, "*");
            strnfmt(s, sizeof(s), "Weak %d", power_counts[0]);
            Term_putstr(legend_col + 4, legend_row + 3, -1, TERM_WHITE, s);
        }
    }
}
/*
 * Player character template selection
 */
static bool get_character_profile(void)
{
    int i;
    int character = 0;
    int character_choice;
    int previous_choice = 0;
    birth_menu* character_menu;

    int no_character_flags = 1;
    for (int idx = 0; idx < FLAG_WORDS; ++idx) {
        if (rp_ptr->choice[idx] != 0) {
            no_character_flags = 0;
            break;  // At least one flag is set
        }
    }
    // default to the baseline character automatically if no choices are available
    if (no_character_flags)
    {
        p_ptr->pcharacter = 0;
        current_character_profile = &c_info[p_ptr->pcharacter];
        return (true);
    }

    character_menu = mem_alloc_array(z_info->c_max, birth_menu);

    /* Tabulate characters */

    for (i = 0; i < z_info->c_max; i++)
    {

        /* Analyze */
        if (is_set(i))
        {
            if (highscore_dead(c_name + c_info[i].name)) character_menu[character].ghost = true;
            else character_menu[character].ghost = false;

            character_menu[character].name = c_name + c_info[i].name;
            character_menu[character].text = c_text + c_info[i].text;
            if (p_ptr->pcharacter == i)
                previous_choice = character;
            character++;
        }
    }

    screen_push_touch_pane_proto();
    character_choice = get_player_choice(
        character_menu, character, previous_choice, CLASS_COL, 22,
        character_aux_hook, true);
    screen_pop_touch_pane_proto();

    /* No selection? */
    if (character_choice == INVALID_CHOICE)
    {
        character_menu = mem_free(character_menu);
        return (false);
    }

    /* Get character from choice number */
    character = 0;
    for (i = 0; i < z_info->c_max; i++)
    {
        if (is_set(i))
        {
            if (character_choice == character)
            {
                // if different character to last time, then wipe the history, age,
                // height, weight
                if (i != p_ptr->pcharacter)
                {
                    int j;

                    p_ptr->history[0] = '\0';
                    p_ptr->age = 0;
                    p_ptr->ht = 0;
                    p_ptr->wt = 0;
                    for (j = 0; j < A_MAX; j++)
                    {
                        p_ptr->stat_base[j] = 0;
                    }
                }
                p_ptr->pcharacter = i;
            }
            character++;
        }
    }

    /* Cache the selected character template */
    current_character_profile = &c_info[p_ptr->pcharacter];

    character_menu = mem_free(character_menu);

    return (true);
}

static cptr blitz_character_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_CHARACTER_RANDOM_STATS: return "Random with stats";
    case BLITZ_CHARACTER_SELECTED: return "Selected";
    default: return "Random";
    }
}

static cptr blitz_effect_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_EFFECT_SELECTED: return "Selected";
    case BLITZ_EFFECT_SELECTED_DESCR: return "Selected + descriptions";
    default: return "Random";
    }
}

static void blitz_setup_clamp(blitz_setup* setup)
{
    if (!setup)
        return;

    if (setup->character_mode > BLITZ_CHARACTER_SELECTED)
        setup->character_mode = BLITZ_CHARACTER_RANDOM;
    if (setup->effect_mode > BLITZ_EFFECT_SELECTED_DESCR)
        setup->effect_mode = BLITZ_EFFECT_RANDOM;
    if (setup->blessing_count > BLITZ_MAX_EFFECT_COUNT)
        setup->blessing_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count > BLITZ_MAX_EFFECT_COUNT)
        setup->curse_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count < setup->blessing_count)
        setup->curse_count = setup->blessing_count;
}

static void blitz_pick_random_race_and_character(void)
{
    int race = 0;
    int available[64];
    int available_count = 0;

    if (!z_info)
        return;

    race = rand_int(z_info->p_max);
    p_ptr->prace = race;
    rp_ptr = &p_info[p_ptr->prace];

    for (int i = 0; i < z_info->c_max && available_count < (int)N_ELEMENTS(available); i++)
    {
        if (is_set(i))
            available[available_count++] = i;
    }

    if (available_count <= 0)
        p_ptr->pcharacter = 0;
    else
        p_ptr->pcharacter = available[rand_int(available_count)];

    current_character_profile = &c_info[p_ptr->pcharacter];
}

static void blitz_setup_draw(const blitz_setup* setup, int selected)
{
    char buf[160];
    int wid = 80;
    int hgt = 24;
    bool steamdeck = steamdeck_controls_active();

    Term_get_size(&wid, &hgt);
    Term_clear();
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    c_put_str(TERM_YELLOW, "Blitz Setup", 1, MAX((wid - 11) / 2, 0));
    birth_put_wrapped_text(TERM_SLATE,
        "Configure a self-contained Blitz run. Story progress stays untouched.",
        3, 2);

    strnfmt(buf, sizeof(buf), "Character: %s", blitz_character_mode_name(setup->character_mode));
    birth_put_str_fit(selected == 0 ? TERM_L_BLUE : TERM_WHITE, buf, 6, 4);
    ui_menu_click_add(0, 4, 6, wid - 8);

    strnfmt(buf, sizeof(buf), "Oaths: %s", setup->oaths_enabled ? "Yes" : "No");
    birth_put_str_fit(selected == 1 ? TERM_L_BLUE : TERM_WHITE, buf, 7, 4);
    ui_menu_click_add(1, 4, 7, wid - 8);

    strnfmt(buf, sizeof(buf), "Blessings: %d", setup->blessing_count);
    birth_put_str_fit(selected == 2 ? TERM_L_BLUE : TERM_WHITE, buf, 8, 4);
    ui_menu_click_add(2, 4, 8, wid - 8);

    strnfmt(buf, sizeof(buf), "Curses: %d", setup->curse_count);
    birth_put_str_fit(selected == 3 ? TERM_L_BLUE : TERM_WHITE, buf, 9, 4);
    ui_menu_click_add(3, 4, 9, wid - 8);

    strnfmt(buf, sizeof(buf), "Effect picks: %s", blitz_effect_mode_name(setup->effect_mode));
    birth_put_str_fit(selected == 4 ? TERM_L_BLUE : TERM_WHITE, buf, 10, 4);
    ui_menu_click_add(4, 4, 10, wid - 8);

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];
        char prompt_buf[96];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt_buf, sizeof(prompt_buf),
            "D-pad navigate/change  %s begin  %s back",
            confirm_label, back_label);
        birth_put_str_fit(TERM_L_DARK, prompt_buf, 13, 2);
        ui_menu_click_add_text_token(-2, 2, 13, prompt_buf, "begin");
        ui_menu_click_add_text_token(-1, 2, 13, prompt_buf, "back");
    }
    else
    {
        cptr prompt_text = "8/2 navigate  4/6 change  Enter begin  Esc back";
        birth_put_str_fit(TERM_L_DARK, prompt_text, 13, 2);
        ui_menu_click_add_text_token(-2, 2, 13, prompt_text, "begin");
        ui_menu_click_add_text_token(-1, 2, 13, prompt_text, "back");
    }
}

static NavResult blitz_setup_menu(void)
{
    blitz_setup* setup = blitz_current_setup_mutable();
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();

    blitz_setup_clamp(setup);

    while (1)
    {
        char key;

        blitz_setup_draw(setup, selected);
        key = inkey();
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < 5)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != selected)
                    {
                        selected = clicked_choice;
                        continue;
                    }
                    key = '6';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
        {
            ui_menu_click_clear();
            return NAV_TO_MAIN;
        }

        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
        {
            ui_menu_click_clear();
            return NAV_OK;
        }

        if (key == '8')
        {
            selected = (selected + 4) % 5;
            continue;
        }

        if (key == '2')
        {
            selected = (selected + 1) % 5;
            continue;
        }

        if (key != '4' && key != '6')
            continue;

        switch (selected)
        {
        case 0:
            if (key == '4')
                setup->character_mode = (setup->character_mode == BLITZ_CHARACTER_RANDOM)
                    ? BLITZ_CHARACTER_SELECTED
                    : setup->character_mode - 1;
            else
                setup->character_mode = (setup->character_mode == BLITZ_CHARACTER_SELECTED)
                    ? BLITZ_CHARACTER_RANDOM
                    : setup->character_mode + 1;
            break;
        case 1:
            setup->oaths_enabled = !setup->oaths_enabled;
            break;
        case 2:
            if (key == '4' && setup->blessing_count > 0)
                setup->blessing_count--;
            else if (key == '6' && setup->blessing_count < BLITZ_MAX_EFFECT_COUNT)
                setup->blessing_count++;
            break;
        case 3:
            if (key == '4' && setup->curse_count > 0)
                setup->curse_count--;
            else if (key == '6' && setup->curse_count < BLITZ_MAX_EFFECT_COUNT)
                setup->curse_count++;
            break;
        case 4:
            if (key == '4')
                setup->effect_mode = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
                    ? BLITZ_EFFECT_SELECTED_DESCR
                    : setup->effect_mode - 1;
            else
                setup->effect_mode = (setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR)
                    ? BLITZ_EFFECT_RANDOM
                    : setup->effect_mode + 1;
            break;
        default:
            break;
        }

        blitz_setup_clamp(setup);
    }
}

static void finalize_character_creation_selection(void)
{
    int i, j;

    /* Clear the base values of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_base[i] = 0;

    /* Clear the abilities and add bonus ability*/
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[p_ptr->pcharacter].a_adj[slot][0];
        int ab;

        if (stat < 0) break;
        ab = c_info[p_ptr->pcharacter].a_adj[slot][1];
        if (stat < S_MAX && ab < ABILITIES_MAX)
        {
            p_ptr->innate_ability[stat][ab] = true;
            p_ptr->active_ability[stat][ab] = true;
        }
    }

    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
        op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];

    for (i = OPT_CHEAT; i < OPT_ADULT; i++)
        op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];

    if (strlen(op_ptr->full_name) == 0)
    {
        op_ptr->vault_drop_frequency = VDF_NORMAL;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    }

    if (op_ptr->main_combat_rolls > 4)
        op_ptr->main_combat_rolls = 0;
    if (op_ptr->ability_desc_mode > 2)
        op_ptr->ability_desc_mode = 0;
    if (op_ptr->vault_drop_frequency > VDF_PLENTIFUL)
        op_ptr->vault_drop_frequency = VDF_NORMAL;
    if (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_OFF)
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
    if (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_OFF)
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;
    if (op_ptr->narrative_banner_turns > NARRATIVE_BANNER_TURNS_MAX)
        op_ptr->narrative_banner_turns = DEFAULT_NARRATIVE_BANNER_TURNS;
    if (op_ptr->intro_style > INTRO_STYLE_RANDOM)
        op_ptr->intro_style = INTRO_STYLE_RANDOM;
    if (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    if (op_ptr->min_depth_timer_mode > MIN_DEPTH_TIMER_MODE_MAX)
        op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;

    for (i = 0; i < z_info->k_max; i++)
        k_info[i].squelch = SQUELCH_NEVER;
    for (i = 0; i < SQUELCH_BYTES; i++)
        squelch_level[i] = SQUELCH_NONE;
    for (i = 0; i < z_info->e_max; i++)
    {
        e_info[i].aware = false;
        e_info[i].squelch = false;
    }

    Term_clear();

    log_debug("Character creation step completed: %s %s",
        p_name + p_info[p_ptr->prace].name,
        c_name + c_info[p_ptr->pcharacter].name);
}

NavResult blitz_character_creation(void)
{
    blitz_runtime_reset();

    if (blitz_setup_menu() != NAV_OK)
        return NAV_TO_MAIN;

    if (blitz_current_setup()->character_mode == BLITZ_CHARACTER_SELECTED)
        return character_creation();

    blitz_pick_random_race_and_character();
    finalize_character_creation_selection();
    return NAV_OK;
}

/*
 * Helper function for 'player_birth()'.
 *
 * This function allows the player to select a race and character template, and
 * modify options (including the birth options).
 */
NavResult character_creation(void)
{
    int i;

    int phase = 1;
    NavResult result = NAV_OK;

    screen_push_touch_pane_proto();

    /*** Instructions ***/

    /* Clear screen */
    Term_clear();

    /* Display some helpful information */
    draw_character_selection_header(false);

    if (steamdeck_controls_active()) {
        int prompt_row = birth_prompt_row();
        char random_label[16];
        char back_label[16];
        char options_label[16];
        char scores_label[16];
        char full_desc_label[16];
        char help_label[16];
        char quit_label[16];
        char prompt_buf[160];

        birth_prompt_label('r', "r", random_label, sizeof(random_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('o', "o", options_label, sizeof(options_label));
        birth_prompt_label('s', "s", scores_label, sizeof(scores_label));
        birth_prompt_label(steamdeck_alt_action_key(), "X", full_desc_label,
            sizeof(full_desc_label));
        birth_prompt_label('?', "?", help_label, sizeof(help_label));
        if (streq(help_label, "?"))
            birth_prompt_label('h', "h", help_label, sizeof(help_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        strnfmt(prompt_buf, sizeof(prompt_buf),
            "%s-random  %s-back  %s-options  %s-scores  %s-description  %s-help  %s-quit",
            random_label, back_label, options_label, scores_label, full_desc_label, help_label, quit_label);
        Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt_buf);
    } else {
        Term_putstr(QUESTION_COL, birth_prompt_row(), -1, TERM_SLATE,
            "r -random   ESC -back   o -options   s -scores   f -description   h -help   q -quit");
    }

    while (phase <= 2)
    {
        clear_question();

        if (phase == 1)
        {
            /* Choose the player's race */
            if (!get_player_race())
            {
                result = NAV_TO_MAIN; /* Esc at first screen -> back to main menu */
                goto cleanup;
            }

            /* Clean up */
            clear_question();

            phase++;
        }

        if (phase == 2)
        {
            draw_character_selection_header(true);

            /* Choose the player's character template */
            if (!get_character_profile())
            {
                phase = 1;          /* Esc here -> go back to race */
                draw_character_selection_header(false);
                /* Clear the character display area when going back to race selection */
                for (i = HEADER_ROW; i <= TABLE_ROW + A_MAX + 10; i++)
                {
                    Term_erase(TOTAL_AUX_COL, i, 255);
                }
                continue;
            }

            /* Clean up */
            clear_question();

            phase++;
        }
    }

    finalize_character_creation_selection();

    /* Done */
    result = NAV_OK;

cleanup:
    ui_menu_click_clear();
    screen_pop_touch_pane_proto();
    return result;

}

static bool oath_menu_use_compact_layout(void)
{
    int wid = 80;
    int hgt = 24;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    return (wid < 80) || (hgt < 24);
}

static int oath_selectable_max_id(void)
{
    int max_oath_id = OATH_LIGHT;

    if (!z_info)
        return max_oath_id;
    if (z_info->oath_max <= 1)
        return 0;
    if (max_oath_id >= z_info->oath_max)
        max_oath_id = z_info->oath_max - 1;
    if (max_oath_id < 0)
        max_oath_id = 0;

    return max_oath_id;
}

static int oath_collect_visible(int available_mask, int* visible_oaths, int max_visible)
{
    int visible_count = 0;
    int max_oath_id = oath_selectable_max_id();

    if (visible_oaths && visible_count < max_visible)
        visible_oaths[visible_count] = 0;
    visible_count++;

    for (int i = 1; i <= max_oath_id; i++)
    {
        if (!(available_mask & (1 << (i - 1))) && !oath_banned(i))
            continue;

        if (visible_oaths && visible_count < max_visible)
            visible_oaths[visible_count] = i;

        visible_count++;
    }

    return visible_count;
}

static bool oath_option_selectable(int oath_id, int available_mask)
{
    if (oath_id == 0)
        return true;

    return ((available_mask & (1 << (oath_id - 1))) != 0) && !oath_banned(oath_id);
}

static void oath_move_highlight(int* highlight, int direction, int available_mask)
{
    int oath_max = oath_selectable_max_id() + 1;
    int original = *highlight;
    int next = *highlight;

    if (oath_max <= 0)
    {
        *highlight = 0;
        return;
    }

    do
    {
        next += direction;
        if (next < 0)
            next = oath_max - 1;
        if (next >= oath_max)
            next = 0;

        if ((next == 0)
            || (available_mask & (1 << (next - 1)))
            || oath_banned(next))
        {
            *highlight = next;
            return;
        }
    } while (next != original);
}

static void oath_center_putstr(int row, byte attr, cptr text)
{
    int wid = 80;
    int hgt = 24;
    int col;

    if (!text)
        text = "";

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;

    col = (wid - (int)strlen(text)) / 2;
    if (col < 0)
        col = 0;

    Term_putstr(col, row, -1, attr, text);
}

static void oath_draw_page_indicator(int page, int page_count, int wid, int row)
{
    char page_buf[16];
    int col;

    if (page_count <= 1)
        return;

    strnfmt(page_buf, sizeof(page_buf), "%d/%d", page + 1, page_count);
    col = wid - (int)strlen(page_buf) - 1;
    if (col < 0)
        col = 0;

    Term_putstr(col, row, -1, TERM_WHITE, page_buf);
}

static void oath_putstr_fit(int col, int row, int max_width, byte attr, cptr text)
{
    char buf[256];
    int len;

    if (max_width <= 0)
        return;

    if (!text)
        text = "";

    len = (int)strlen(text);
    if (len <= max_width)
    {
        Term_putstr(col, row, -1, attr, text);
        return;
    }

    if (max_width <= 3)
        strnfmt(buf, sizeof(buf), "%.*s", max_width, text);
    else
        strnfmt(buf, sizeof(buf), "%.*s...", max_width - 3, text);

    Term_putstr(col, row, -1, attr, buf);
}

static void oath_render_virtual_line(int col, int max_width, int* draw_row,
    int row_limit, int* virtual_row, int skip_lines, byte color, cptr text,
    bool render)
{
    if (!text)
        text = "";

    if ((*virtual_row >= skip_lines) && render && (*draw_row < row_limit))
        oath_putstr_fit(col, *draw_row, max_width, color, text);

    if ((*virtual_row >= skip_lines) && (*draw_row < row_limit))
        (*draw_row)++;

    (*virtual_row)++;
}

static void oath_render_virtual_wrapped_text(cptr text, int col, int max_width,
    int* draw_row, int row_limit, int* virtual_row, int skip_lines,
    byte color, bool render)
{
    char line_buffer[512];
    int line_pos = 0;
    const char* text_ptr = text;

    if (!text || !text[0])
        return;

    if (max_width <= 0)
    {
        int term_width = 80;
        int term_height = 24;

        Term_get_size(&term_width, &term_height);
        (void)term_height;
        if (term_width < 1)
            term_width = 80;
        max_width = term_width - col - 2;
    }

    if (max_width < 8)
        max_width = 8;

    while (*text_ptr)
    {
        while (*text_ptr == ' ' && line_pos == 0)
            text_ptr++;

        if (*text_ptr == '\n')
        {
            line_buffer[line_pos] = '\0';
            if (line_pos > 0)
                oath_render_virtual_line(col, max_width, draw_row, row_limit, virtual_row,
                    skip_lines, color, line_buffer, render);
            else
                oath_render_virtual_line(col, max_width, draw_row, row_limit, virtual_row,
                    skip_lines, color, "", render);

            line_pos = 0;
            text_ptr++;
            continue;
        }

        if ((line_pos >= max_width) || (line_pos >= (int)sizeof(line_buffer) - 1))
        {
            int wrap_pos = line_pos - 1;

            while (wrap_pos > 0 && line_buffer[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;

                line_buffer[wrap_pos] = '\0';
                oath_render_virtual_line(col, max_width, draw_row, row_limit, virtual_row,
                    skip_lines, color, line_buffer, render);

                for (int i = 0; i < remaining; i++)
                    line_buffer[i] = line_buffer[wrap_pos + 1 + i];

                line_pos = remaining;
            }
            else
            {
                line_buffer[line_pos] = '\0';
                oath_render_virtual_line(col, max_width, draw_row, row_limit, virtual_row,
                    skip_lines, color, line_buffer, render);
                line_pos = 0;
            }

            continue;
        }

        line_buffer[line_pos++] = *text_ptr++;
    }

    if (line_pos > 0)
    {
        line_buffer[line_pos] = '\0';
        oath_render_virtual_line(col, max_width, draw_row, row_limit, virtual_row,
            skip_lines, color, line_buffer, render);
    }
}

static int oath_render_detail_content(int oath_id, int col, int start_row,
    int max_width, int row_limit, int skip_lines, bool render)
{
    int draw_row = start_row;
    int virtual_row = 0;
    char line_buf[768];

    if (oath_id < 0 || !z_info || oath_id >= z_info->oath_max)
        return 0;

    if (oath_banned(oath_id) && oath_id > 0)
    {
        char* banned_text = oath_banned_text(oath_id);

        oath_render_virtual_line(col, max_width, &draw_row, row_limit, &virtual_row,
            skip_lines, TERM_L_RED, "OATH BROKEN", render);

        if (banned_text && banned_text[0])
        {
            oath_render_virtual_wrapped_text(banned_text, col, max_width,
                &draw_row, row_limit, &virtual_row, skip_lines, TERM_RED, render);
        }
        else
        {
            oath_render_virtual_wrapped_text(
                "Thy oath lies shattered, and thy name is marked in shame for this age.",
                col, max_width, &draw_row, row_limit, &virtual_row, skip_lines,
                TERM_RED, render);
        }

        return virtual_row;
    }

    if (oath_id == 0)
    {
        oath_render_virtual_wrapped_text("Walk free of binding words.", col,
            max_width, &draw_row, row_limit, &virtual_row, skip_lines,
            TERM_SLATE, render);
        oath_render_virtual_wrapped_text(
            "Take no oath and remain unbound by sacred vows.", col, max_width,
            &draw_row, row_limit, &virtual_row, skip_lines, TERM_SLATE, render);
        return virtual_row;
    }

    if (oath_description(oath_id) && oath_description(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Description: %s",
            oath_description(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_SLATE, render);
    }

    if (oath_pledge(oath_id) && oath_pledge(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Pledge: %s", oath_pledge(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_L_BLUE, render);
    }

    if (oath_reward_text(oath_id) && oath_reward_text(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Reward: %s",
            oath_reward_text(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_L_GREEN, render);
    }

    if (oath_forbidden(oath_id) && oath_forbidden(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Forbidden: %s",
            oath_forbidden(oath_id));
        oath_render_virtual_wrapped_text(line_buf, col, max_width, &draw_row,
            row_limit, &virtual_row, skip_lines, TERM_L_RED, render);
    }

    return virtual_row;
}

static void oath_render_wrapped_block(cptr text, int col, int max_width,
    int* draw_row, int row_limit, byte color)
{
    int virtual_row = 0;

    oath_render_virtual_wrapped_text(text, col, max_width, draw_row, row_limit,
        &virtual_row, 0, color, true);
}

static void oath_draw_compact_list_summary(int oath_id, int row, int prompt_row,
    int max_width)
{
    char line_buf[512];
    byte name_attr = TERM_L_BLUE;

    if (row >= prompt_row || max_width <= 0)
        return;

    if (oath_id == 0)
        name_attr = TERM_WHITE;
    else if (oath_banned(oath_id))
        name_attr = TERM_L_RED;

    oath_putstr_fit(2, row++, max_width, name_attr, oath_name_str(oath_id));

    if (row >= prompt_row)
        return;

    if (oath_id == 0)
    {
        oath_putstr_fit(2, row, max_width, TERM_SLATE,
            "No oath. No restrictions.");
        return;
    }

    if (oath_banned(oath_id))
    {
        oath_putstr_fit(2, row, max_width, TERM_RED,
            "Broken oath: unavailable for this metarun.");
        return;
    }

    if (oath_reward_text(oath_id) && oath_reward_text(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Reward: %s",
            oath_reward_text(oath_id));
        oath_render_wrapped_block(line_buf, 2, max_width, &row, prompt_row,
            TERM_L_GREEN);
    }

    if (row >= prompt_row)
        return;

    if (oath_forbidden(oath_id) && oath_forbidden(oath_id)[0])
    {
        strnfmt(line_buf, sizeof(line_buf), "Forbidden: %s",
            oath_forbidden(oath_id));
        oath_render_wrapped_block(line_buf, 2, max_width, &row, prompt_row,
            TERM_L_RED);
    }
}

/*
 * Oath selection screen.
 *
 * Wide screens keep the split list/details layout. Compact screens use a
 * dedicated list page plus a full-width details page with vertical scrolling.
 */
static NavResult select_oath(void)
{
    enum {
        OATH_CLICK_BACK = -1,
        OATH_CLICK_SELECT = -2,
        OATH_CLICK_DETAILS = -3,
        OATH_CLICK_LIST = -4
    };
    int available_mask = get_available_oaths_mask();

    /* If no oaths are available, skip oath selection */
    if (available_mask == 0)
    {
        p_ptr->oath_type = 0; /* No oath */
        log_debug("No oaths available, skipping oath selection");
        return NAV_OK;
    }

    int highlight = 1; /* Start highlighting first available oath */
    int choice = 0;
    int page = 0;
    int detail_scroll = 0;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    /* Find first available oath to highlight */
    for (int i = 1; i <= oath_selectable_max_id(); i++)
    {
        if (available_mask & (1 << (i - 1)))
        {
            highlight = i;
            break;
        }
    }

    while (true)
    {
        int wid = 80;
        int hgt = 24;
        int prompt_row;
        int visible_oaths[16];
        int visible_count;
        int detail_max_scroll = 0;
        bool compact;
        char key;

        Term_get_size(&wid, &hgt);
        if (wid < 1)
            wid = 80;
        if (hgt < 1)
            hgt = 24;

        prompt_row = hgt - 1;
        if (prompt_row < 0)
            prompt_row = 0;

        compact = oath_menu_use_compact_layout();
        if (!compact)
            page = 0;

        visible_count = oath_collect_visible(available_mask, visible_oaths,
            (int)N_ELEMENTS(visible_oaths));
        if (visible_count > (int)N_ELEMENTS(visible_oaths))
            visible_count = (int)N_ELEMENTS(visible_oaths);

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        if (compact)
        {
            oath_center_putstr(0, TERM_L_BLUE,
                (page == 0) ? "Choose your Oath" : "Oath Details");
            oath_draw_page_indicator(page, 2, wid, 0);

            if (page == 0)
            {
                int list_row = 4;

                Term_putstr(2, 2, -1, TERM_WHITE, "Available Oaths");

                for (int i = 0; i < visible_count; i++)
                {
                    int oath_id = visible_oaths[i];
                    byte attr;
                    char buf[96];

                    if (oath_banned(oath_id) && oath_id > 0)
                        attr = (highlight == oath_id) ? TERM_L_RED : TERM_RED;
                    else
                        attr = (highlight == oath_id) ? TERM_L_BLUE : TERM_WHITE;

                    if (!menu_letters)
                        strnfmt(buf, sizeof(buf), "   %s", oath_name_str(oath_id));
                    else
                        strnfmt(buf, sizeof(buf), "%c) %s", 'a' + i, oath_name_str(oath_id));
                    Term_putstr(2, list_row + i, -1, attr, buf);
                    ui_menu_click_add(oath_id, 2, list_row + i, wid - 4);
                }

                oath_draw_compact_list_summary(highlight, list_row + visible_count + 1,
                    prompt_row, wid - 4);

                if (steamdeck)
                {
                    char confirm_label[16];
                    char back_label[16];
                    char prompt_buf[96];

                    birth_prompt_label(steamdeck_confirm_key(), "A",
                        confirm_label, sizeof(confirm_label));
                    birth_prompt_label(steamdeck_back_key(), "B",
                        back_label, sizeof(back_label));
                    strnfmt(prompt_buf, sizeof(prompt_buf),
                        "D-pad Nav/Page  %s Select  %s Back",
                        confirm_label, back_label);
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE, prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
                else
                {
                    char prompt_buf[96];

                    SDL_strlcpy(prompt_buf,
                        "8/2 Nav  6 Details  Enter/Space Select  Esc Back",
                        sizeof(prompt_buf));
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                        prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_DETAILS, 2,
                        prompt_row, prompt_buf, "Details");
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
            }
            else
            {
                int content_row = 3;
                int visible_rows = prompt_row - content_row;
                int total_lines;

                Term_putstr(2, 1, -1, oath_banned(highlight) ? TERM_L_RED : TERM_L_BLUE,
                    oath_name_str(highlight));

                total_lines = oath_render_detail_content(highlight, 2, content_row,
                    wid - 4, prompt_row, 0, false);

                if (visible_rows < 0)
                    visible_rows = 0;

                detail_max_scroll = (total_lines > visible_rows)
                    ? (total_lines - visible_rows)
                    : 0;

                if (detail_scroll > detail_max_scroll)
                    detail_scroll = detail_max_scroll;

                (void)oath_render_detail_content(highlight, 2, content_row, wid - 4,
                    prompt_row, detail_scroll, true);

                if (detail_max_scroll > 0)
                {
                    char scroll_buf[32];

                    strnfmt(scroll_buf, sizeof(scroll_buf), "Scroll %d/%d",
                        detail_scroll + 1, detail_max_scroll + 1);
                    oath_putstr_fit(2, 2, wid - 4, TERM_SLATE, scroll_buf);
                }

                if (steamdeck)
                {
                    char confirm_label[16];
                    char back_label[16];
                    char prompt_buf[96];

                    birth_prompt_label(steamdeck_confirm_key(), "A",
                        confirm_label, sizeof(confirm_label));
                    birth_prompt_label(steamdeck_back_key(), "B",
                        back_label, sizeof(back_label));
                    strnfmt(prompt_buf, sizeof(prompt_buf),
                        "D-pad Scroll/Page  %s Select  %s Back",
                        confirm_label, back_label);
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE, prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
                else
                {
                    char prompt_buf[96];

                    SDL_strlcpy(prompt_buf,
                        "8/2 Scroll  4 List  Enter/Space Select  Esc Back",
                        sizeof(prompt_buf));
                    oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                        prompt_buf);
                    ui_menu_click_add_text_token(OATH_CLICK_LIST, 2,
                        prompt_row, prompt_buf, "List");
                    ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                        prompt_row, prompt_buf, "Select");
                    ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                        prompt_row, prompt_buf, "Back");
                }
            }
        }
        else
        {
            int footer_row = prompt_row - 3;
            int details_col = COL_DESCRIPTION - 2;
            int details_width;
            int list_width;

            if (details_col < 20)
                details_col = 20;
            if (details_col > wid - 12)
                details_col = wid - 12;

            details_width = wid - details_col - 2;
            list_width = details_col - 4;

            oath_center_putstr(0, TERM_L_BLUE, "Choose your Oath");
            Term_putstr(2, 2, -1, TERM_WHITE, "Available Oaths");
            oath_putstr_fit(details_col, 2, details_width, TERM_WHITE,
                "Oath Details");

            for (int i = 0; i < visible_count; i++)
            {
                int oath_id = visible_oaths[i];
                byte attr;
                char buf[96];

                if (oath_banned(oath_id) && oath_id > 0)
                    attr = (highlight == oath_id) ? TERM_L_RED : TERM_RED;
                else
                    attr = (highlight == oath_id) ? TERM_L_BLUE : TERM_WHITE;

                if (!menu_letters)
                    strnfmt(buf, sizeof(buf), "   %s", oath_name_str(oath_id));
                else
                    strnfmt(buf, sizeof(buf), "%c) %s", 'a' + i, oath_name_str(oath_id));
                oath_putstr_fit(2, 4 + i, list_width, attr, buf);
                ui_menu_click_add(oath_id, 2, 4 + i, list_width);
            }

            (void)oath_render_detail_content(highlight, details_col, 4,
                details_width, prompt_row, 0, true);

            if (footer_row >= 0)
            {
                oath_putstr_fit(2, footer_row, wid - 4, TERM_SLATE,
                    "Oaths grant power, but they bind your actions.");
            }
            if (footer_row + 1 < prompt_row)
            {
                oath_putstr_fit(2, footer_row + 1, wid - 4, TERM_SLATE,
                    "Breaking an oath brings curse and shame.");
            }

            if (steamdeck)
            {
                char confirm_label[16];
                char back_label[16];
                char prompt_buf[128];

                birth_prompt_label(steamdeck_confirm_key(), "A",
                    confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "D-pad Navigate  %s Select  %s Back",
                    confirm_label, back_label);
                oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE, prompt_buf);
                ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                    prompt_row, prompt_buf, "Select");
                ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                    prompt_row, prompt_buf, "Back");
            }
            else
            {
                char prompt_buf[96];

                SDL_strlcpy(prompt_buf,
                    "8/2 Navigate  Enter/Space Select  Esc Back",
                    sizeof(prompt_buf));
                oath_putstr_fit(2, prompt_row, wid - 4, TERM_SLATE,
                    prompt_buf);
                ui_menu_click_add_text_token(OATH_CLICK_SELECT, 2,
                    prompt_row, prompt_buf, "Select");
                ui_menu_click_add_text_token(OATH_CLICK_BACK, 2,
                    prompt_row, prompt_buf, "Back");
            }
        }

        Term_fresh();
        key = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();

                if (clicked_choice > 0 && clicked_choice < z_info->oath_max)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != highlight)
                    {
                        highlight = clicked_choice;
                        detail_scroll = 0;
                        continue;
                    }

                    if (click_action == UI_MENU_CLICK_SECONDARY
                        && compact && page == 0)
                    {
                        page = 1;
                        detail_scroll = 0;
                        continue;
                    }

                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else
                {
                    switch (clicked_choice)
                    {
                    case OATH_CLICK_BACK: key = ESCAPE; break;
                    case OATH_CLICK_SELECT: key = '\r'; break;
                    case OATH_CLICK_DETAILS: key = '6'; break;
                    case OATH_CLICK_LIST: key = '4'; break;
                    default: break;
                    }
                }
            }
        }

        if (steamdeck && key == steamdeck_back_key())
        {
            ui_menu_click_clear();
            return NAV_BACK; /* Go back to character creation */
        }
        if (key == ESCAPE || key == 'q')
        {
            ui_menu_click_clear();
            return NAV_BACK; /* Go back to character creation */
        }

        if (compact && key == '4')
        {
            page = 0;
            continue;
        }

        if (compact && key == '6')
        {
            page = 1;
            continue;
        }

        if (birth_confirm_input(key, steamdeck)
            || (!compact && key == '6'))
        {
            /* Select current highlighted option */
            if (oath_option_selectable(highlight, available_mask))
            {
                choice = highlight;
                break;
            }
        }

        if (menu_letters && key >= 'a' && key < 'a' + visible_count)
        {
            int display_pos = key - 'a';

            if (display_pos >= 0 && display_pos < visible_count
                && oath_option_selectable(visible_oaths[display_pos], available_mask))
            {
                choice = visible_oaths[display_pos];
                break;
            }

            continue;
        }

        if (key == '8')
        {
            if (compact && page == 1)
            {
                if (detail_scroll > 0)
                    detail_scroll--;
                continue;
            }

            oath_move_highlight(&highlight, -1, available_mask);
            detail_scroll = 0;
        }

        if (key == '2')
        {
            if (compact && page == 1)
            {
                if (detail_scroll < detail_max_scroll)
                    detail_scroll++;
                continue;
            }

            oath_move_highlight(&highlight, 1, available_mask);
            detail_scroll = 0;
        }
    }

    /* Set the chosen oath */
    ui_menu_click_clear();
    p_ptr->oath_type = choice;
    
    /* Grant corresponding oath special ability using oath.txt data */
    if (choice > 0 && choice < z_info->oath_max) 
    {
        oath_type *oath_ptr = &oath_info[choice];
        
        /* Apply ability reward from A: field in oath.txt */
        if (oath_ptr->reward_type > 0 && oath_ptr->reward_value > 0)
        {
            int skill_category = oath_ptr->reward_type;
            int ability_id = oath_ptr->reward_value;
            
            if (skill_category >= 0 && skill_category < S_MAX
                && ability_id >= 0 && ability_id < ABILITIES_MAX)
            {
                /* Grant the ability specified in oath.txt */
                p_ptr->have_ability[skill_category][ability_id] = true;
                p_ptr->innate_ability[skill_category][ability_id] = true;
                p_ptr->active_ability[skill_category][ability_id] = true;

                log_debug("Granted oath %d abilities from data: skill=%d, ability=%d",
                          choice, skill_category, ability_id);
            }
            else
            {
                log_warn("Oath %d ability out of bounds: skill=%d (max %d), ability=%d (max %d)",
                         choice, skill_category, S_MAX - 1, ability_id, ABILITIES_MAX - 1);
            }
        }
        else
        {
            log_debug("No ability reward found for oath %d", choice);
        }
    }
    
    if (choice == 0) {
        log_debug("No oath selected");
    } else {
        log_debug("Oath selected: %s (%d)", oath_name_str(choice), choice);
    }
    
    return NAV_OK;
}

/*
 * Initial stat costs.
 */
static const int birth_stat_costs[11]
    = { -4, -3, -2, -1, 0, 1, 3, 6, 10, 15, 21 };

#define MAX_COST 13

static int birth_stat_increase_cost(int stat)
{
    int current_index = stat + 4;
    int next_index = current_index + 1;

    if (current_index < 0 || next_index < 0)
        return 0;
    if (next_index >= (int)N_ELEMENTS(birth_stat_costs))
        return 0;

    return birth_stat_costs[next_index] - birth_stat_costs[current_index];
}

/* Forward declaration: used by compact skill allocation rendering. */
static int skill_cost(int base, int points);

static cptr blitz_curse_name_str(int id)
{
    cptr raw = cu_name + cu_info[id].name;
    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    return raw;
}

static cptr blitz_blessing_name_str(int id)
{
    if (cu_info[id].blessing_name)
    {
        cptr raw = cu_name + cu_info[id].blessing_name;
        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;
        return raw;
    }

    return blitz_curse_name_str(id);
}

static int blitz_collect_eligible_effect_ids(bool blessing, int ids[], int max_ids)
{
    int count = 0;

    for (int id = 0; z_info && id < z_info->cu_max && count < max_ids; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int curse_stacks = (stacks > 0) ? stacks : 0;
        byte curse_cap = (byte)CURSE_CURSE_CAP(id);
        byte blessing_cap = (byte)CURSE_BLESSING_CAP(id);

        if (blessing)
        {
            if (!cu_info[id].blessing_name)
                continue;
            if (stacks > 0)
                continue;
            if (blessing_cap > 0 && blessing_stacks >= blessing_cap)
                continue;
        }
        else
        {
            if (!cu_info[id].name)
                continue;
            if (curse_cap > 0 && curse_stacks >= curse_cap)
                continue;
        }

        ids[count++] = id;
    }

    return count;
}

static int blitz_weighted_random_curse_pick(void)
{
    long total = 0;
    int w_max = 1;
    bool tilt = (p_info[p_ptr->prace].flags & RHF_CURSE)
        || (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name)
            continue;
        if (cu_info[i].weight > w_max)
            w_max = cu_info[i].weight;
    }

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        total += base / (cnt + 1);
    }

    if (!total)
        return -1;

    long pick = rand_int(total);
    long run = 0;
    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;
        long eff;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        eff = base / (cnt + 1);
        run += eff;
        if (pick < run)
            return i;
    }

    return -1;
}

static int blitz_weighted_random_blessing_pick(void)
{
    int eligible[METAR_CURSE_SLOTS];
    int weights[METAR_CURSE_SLOTS];
    int count = 0;
    int total_weight = 0;

    for (int id = 0; z_info && id < z_info->cu_max && count < METAR_CURSE_SLOTS; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int base_weight;
        int effective_weight;

        if (!cu_info[id].blessing_name)
            continue;
        if (stacks > 0)
            continue;
        if (CURSE_BLESSING_CAP(id) > 0
            && blessing_stacks >= CURSE_BLESSING_CAP(id))
            continue;

        eligible[count] = id;
        base_weight = cu_info[id].weight > 0 ? cu_info[id].weight : 1;
        effective_weight = base_weight / (blessing_stacks + 1);
        weights[count] = (effective_weight > 0) ? effective_weight : 1;
        total_weight += weights[count];
        count++;
    }

    if (count <= 0 || total_weight <= 0)
        return -1;

    int roll = rand_int(total_weight);
    int sum = 0;
    for (int i = 0; i < count; i++)
    {
        sum += weights[i];
        if (roll < sum)
            return eligible[i];
    }

    return eligible[0];
}

static int blitz_select_effect_from_list(bool blessing, bool show_effects, int ordinal, int total)
{
    int ids[METAR_CURSE_SLOTS];
    int count = blitz_collect_eligible_effect_ids(blessing, ids, METAR_CURSE_SLOTS);
    int selected = 0;
    int top = 0;
    bool steamdeck = steamdeck_controls_active();

    if (count <= 0)
        return -1;

    while (1)
    {
        int wid = 80;
        int hgt = 24;
        int list_rows;
        int selected_id;
        int row;
        char key;
        char title[80];

        Term_get_size(&wid, &hgt);
        list_rows = show_effects ? MAX(4, hgt - 11) : MAX(4, hgt - 10);

        if (selected < top)
            top = selected;
        if (selected >= top + list_rows)
            top = selected - list_rows + 1;

        selected_id = ids[selected];
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        strnfmt(title, sizeof(title), "Choose %s %d of %d",
            blessing ? "Blessing" : "Curse", ordinal, total);
        c_put_str(TERM_YELLOW, title, 1, MAX((wid - (int)strlen(title)) / 2, 0));

        for (row = 0; row < list_rows && top + row < count; row++)
        {
            int idx = top + row;
            cptr name = blessing ? blitz_blessing_name_str(ids[idx])
                                 : blitz_curse_name_str(ids[idx]);
            char line[128];
            strnfmt(line, sizeof(line), "%s", name);
            birth_put_str_fit(idx == selected ? TERM_L_BLUE : (blessing ? TERM_L_GREEN : TERM_L_RED),
                line, 3 + row, 4);
            ui_menu_click_add(idx, 4, 3 + row, wid - 8);
        }

        {
            curse_type* cu = &cu_info[selected_id];
            cptr desc = blessing
                ? (cu->blessing_text ? cu_text + cu->blessing_text : "")
                : (cu->text ? cu_text + cu->text : "");
            cptr power = blessing
                ? (cu->blessing_power ? cu_text + cu->blessing_power : "")
                : (cu->power ? cu_text + cu->power : "");
            int desc_row = 4 + list_rows;

            birth_put_str_fit(TERM_WHITE, blessing ? blitz_blessing_name_str(selected_id)
                                                   : blitz_curse_name_str(selected_id),
                desc_row++, 2);
            if (desc && desc[0])
            {
                birth_put_wrapped_text(TERM_SLATE, desc, desc_row, 2);
                desc_row += birth_wrapped_line_count(desc, 2);
            }
            if (show_effects && power && power[0])
            {
                char power_line[512];
                strnfmt(power_line, sizeof(power_line), "Effect: %s", power);
                birth_put_wrapped_text(blessing ? TERM_L_GREEN : TERM_L_RED,
                    power_line, desc_row + 1, 2);
            }
        }

        if (steamdeck)
        {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[96];

            birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
                sizeof(confirm_label));
            birth_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt_buf, sizeof(prompt_buf),
                "D-pad navigate  %s select  %s back",
                confirm_label, back_label);
            birth_put_str_fit(TERM_L_DARK, prompt_buf, hgt - 1, 2);
            ui_menu_click_add_text_token(-2, 2, hgt - 1, prompt_buf,
                "select");
            ui_menu_click_add_text_token(-1, 2, hgt - 1, prompt_buf,
                "back");
        }
        else
        {
            cptr prompt_text = "8/2 navigate  Enter select  Esc back";
            birth_put_str_fit(TERM_L_DARK, prompt_text, hgt - 1, 2);
            ui_menu_click_add_text_token(-2, 2, hgt - 1, prompt_text,
                "select");
            ui_menu_click_add_text_token(-1, 2, hgt - 1, prompt_text,
                "back");
        }
        key = inkey();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != selected)
                    {
                        selected = clicked_choice;
                        continue;
                    }
                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
        {
            ui_menu_click_clear();
            return -1;
        }
        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
        {
            ui_menu_click_clear();
            return selected_id;
        }
        if (key == '8')
        {
            selected = (selected + count - 1) % count;
            continue;
        }
        if (key == '2')
        {
            selected = (selected + 1) % count;
            continue;
        }
    }
}

static void blitz_apply_effect_pick(int id, bool blessing)
{
    CURSE_ADD(id, blessing ? -1 : 1);
    CURSE_SEEN_SET(id);
}

static void blitz_show_effect_summary(void)
{
    int wid = 80;
    int hgt = 24;
    int row = 3;

    Term_get_size(&wid, &hgt);
    Term_clear();
    c_put_str(TERM_YELLOW, "Blitz Effects", 1, MAX((wid - 13) / 2, 0));

    for (int id = 0; z_info && id < z_info->cu_max; id++)
    {
        int stacks = CURSE_GET(id);
        char line[128];

        if (stacks == 0)
            continue;

        strnfmt(line, sizeof(line), "%s x%d",
            (stacks < 0) ? blitz_blessing_name_str(id) : blitz_curse_name_str(id),
            (stacks < 0) ? -stacks : stacks);
        birth_put_str_fit(stacks < 0 ? TERM_L_GREEN : TERM_L_RED, line, row++, 4);
    }

    if (row == 3)
        birth_put_str_fit(TERM_SLATE, "No blessings or curses selected.", row++, 4);

    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char prompt_buf[48];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] continue", confirm_label);
        birth_put_str_fit(TERM_L_BLUE, prompt_buf, MIN(row + 1, hgt - 1), 2);
    }
    else
    {
        birth_put_str_fit(TERM_L_BLUE, "Press any key to continue.",
            MIN(row + 1, hgt - 1), 2);
    }
    (void)inkey();
}

static NavResult blitz_configure_effects(void)
{
    const blitz_setup* setup = blitz_current_setup();

    blitz_runtime_reset();

    for (int i = 0; i < setup->curse_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_curse_pick()
            : blitz_select_effect_from_list(false,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1, setup->curse_count);
        if (id < 0)
            return NAV_BACK;
        blitz_apply_effect_pick(id, false);
    }

    for (int i = 0; i < setup->blessing_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_blessing_pick()
            : blitz_select_effect_from_list(true,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1, setup->blessing_count);
        if (id < 0)
            return NAV_BACK;
        blitz_apply_effect_pick(id, true);
    }

    if (setup->curse_count > 0 || setup->blessing_count > 0)
        blitz_show_effect_summary();

    return NAV_OK;
}

static void blitz_auto_assign_stats(int stats[A_MAX])
{
    int cost = 0;

    for (int i = 0; i < A_MAX; i++)
        stats[i] = 0;

    while (cost < MAX_COST)
    {
        int choices[A_MAX];
        int choice_count = 0;

        for (int i = 0; i < A_MAX; i++)
        {
            int next = stats[i] + 1;
            int next_cost;

            if (next > 6)
                continue;
            next_cost = cost - birth_stat_costs[stats[i] + 4]
                + birth_stat_costs[next + 4];
            if (next_cost <= MAX_COST)
                choices[choice_count++] = i;
        }

        if (choice_count <= 0)
            break;

        int pick = choices[rand_int(choice_count)];
        cost -= birth_stat_costs[stats[pick] + 4];
        stats[pick]++;
        cost += birth_stat_costs[stats[pick] + 4];
    }
}

static void blitz_auto_assign_skills(void)
{
    int old_base[S_MAX];
    int gains[S_MAX];
    int budget;

    for (int i = 0; i < S_MAX; i++)
    {
        old_base[i] = p_ptr->skill_base[i];
        gains[i] = 0;
    }

    budget = p_ptr->new_exp;

    while (budget > 0)
    {
        int choices[S_MAX];
        int weights[S_MAX];
        int choice_count = 0;
        int total_weight = 0;

        for (int i = 0; i < S_MAX; i++)
        {
            int delta;
            int weight = 2;

            if (i == S_SPC)
                continue;

            delta = skill_cost(old_base[i], gains[i] + 1)
                - skill_cost(old_base[i], gains[i]);
            if (delta <= 0 || delta > budget)
                continue;

            for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
            {
                int skill_idx = c_info[p_ptr->pcharacter].a_adj[slot][0];
                if (skill_idx < 0)
                    break;
                if (skill_idx == i)
                    weight += 3;
            }

            choices[choice_count] = i;
            weights[choice_count] = weight;
            total_weight += weight;
            choice_count++;
        }

        if (choice_count <= 0)
            break;

        int roll = rand_int(total_weight);
        int sum = 0;
        int chosen = choices[0];
        for (int i = 0; i < choice_count; i++)
        {
            sum += weights[i];
            if (roll < sum)
            {
                chosen = choices[i];
                break;
            }
        }

        budget -= skill_cost(old_base[chosen], gains[chosen] + 1)
            - skill_cost(old_base[chosen], gains[chosen]);
        gains[chosen]++;
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (i == S_SPC)
            continue;
        p_ptr->skill_base[i] = old_base[i] + gains[i];
    }

    p_ptr->new_exp = budget;
}

static NavResult blitz_auto_build_character(void)
{
    int stats[A_MAX];

    get_extra();
    blitz_auto_assign_stats(stats);

    for (int i = 0; i < A_MAX; i++)
    {
        int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);
        p_ptr->stat_base[i] = stats[i] + bonus;
        p_ptr->stat_drain[i] = 0;
    }

    p_ptr->update |= (PU_BONUS | PU_HP);
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    calc_voice();
    p_ptr->csp = p_ptr->msp;

    blitz_auto_assign_skills();
    p_ptr->update |= (PU_BONUS);
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    calc_voice();
    p_ptr->csp = p_ptr->msp;

    return NAV_OK;
}

static void birth_register_allocation_prompt_clicks(int row, cptr prompt,
    int col, cptr back_label, cptr confirm_label, cptr quit_label)
{
    if (!prompt)
        return;

    ui_menu_click_add_text_token(-1, col, row, prompt, back_label);
    ui_menu_click_add_text_token(-1, col, row, prompt, "back");
    ui_menu_click_add_text_token(-2, col, row, prompt, confirm_label);
    ui_menu_click_add_text_token(-2, col, row, prompt, "Enter");
    ui_menu_click_add_text_token(-2, col, row, prompt, "enter");
    ui_menu_click_add_text_token(-2, col, row, prompt, "ok");
    ui_menu_click_add_text_token(-2, col, row, prompt, "confirm");
    ui_menu_click_add_text_token(-2, col, row, prompt, "SPACE/ENTER");
    ui_menu_click_add_text_token(-3, col, row, prompt, quit_label);
    ui_menu_click_add_text_token(-3, col, row, prompt, "quit");
    ui_menu_click_add_text_token(-3, col, row, prompt, "char");
    ui_menu_click_add_text_token(-3, col, row, prompt, "character");
    ui_menu_click_add_text_token(-3, col, row, prompt, "selection");
}

static void birth_draw_allocation_confirm_status(int row, int col, int end_col,
    cptr status)
{
    int wid = 80;
    int hgt = 24;
    char status_buf[80];
    cptr back_text = "[Esc]";
    cptr confirm_text = "[Confirm]";
    int back_len = (int)strlen(back_text);
    int confirm_len = (int)strlen(confirm_text);
    int controls_len = back_len + 1 + confirm_len;
    int clear_width;
    int controls_col;
    int confirm_col;
    int status_width;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    if (row < 0 || row >= hgt)
        return;
    if (col < 0)
        col = 0;
    if (end_col >= wid)
        end_col = wid - 1;
    if (end_col < col)
        return;

    clear_width = end_col - col + 1;
    Term_erase(col, row, clear_width);

    if (clear_width < controls_len)
        return;

    controls_col = end_col - controls_len + 1;
    confirm_col = controls_col;
    status_width = controls_col - col - 1;

    if (status && status[0] && status_width > 0)
    {
        SDL_strlcpy(status_buf, status, sizeof(status_buf));
        if ((int)strlen(status_buf) > status_width)
            status_buf[status_width] = '\0';
        Term_putstr(col, row, status_width, TERM_L_BLUE, status_buf);
    }

    Term_putstr(controls_col, row, controls_len, TERM_SLATE,
        "[Confirm] [Esc]");
    ui_menu_click_add(-2, confirm_col, row, confirm_len);
    ui_menu_click_add_text_token(-2, confirm_col, row, confirm_text,
        "Confirm");
    {
        int back_col = confirm_col + confirm_len + 1;

        ui_menu_click_add(-1, back_col, row, back_len);
        ui_menu_click_add_text_token(-1, back_col, row, back_text, "Esc");
    }
}

#define BIRTH_ALLOCATION_DEFAULT_SKILL_ROW 6
#define BIRTH_ALLOCATION_MIN_SKILL_ROW 5
#define BIRTH_ALLOCATION_MIN_HISTORY_ROWS 3

static int birth_allocation_skill_rows(void)
{
    int rows = 0;

    for (int skill = 0; skill < S_MAX; skill++)
    {
        if (skill != S_SPC)
            rows++;
    }

    return rows;
}

static int birth_allocation_history_lines(int wid)
{
    int wrap_width;

    if (!p_ptr || !p_ptr->history[0])
        return 0;

    wrap_width = wid - 1;
    if (wrap_width < 10)
        wrap_width = 10;

    return count_wrapped_lines(p_ptr->history, wrap_width, 1);
}

static int birth_allocation_history_room(int history_row, int body_last_row)
{
    if (history_row < 0 || history_row > body_last_row)
        return 0;

    return body_last_row - history_row + 1;
}

static void birth_configure_allocation_sheet_layout(bool stats_screen,
    int* skill_first_row_out, int* status_row_out)
{
    int wid = 80;
    int hgt = 24;
    int prompt_row;
    int body_last_row;
    int skill_rows;
    int history_lines;
    int skill_first_row = BIRTH_ALLOCATION_DEFAULT_SKILL_ROW;
    int status_row = -1;
    int history_row = -1;
    int best_skill_first_row = skill_first_row;
    int best_status_row = -1;
    int best_history_row = -1;
    int best_history_room = -1;
    int best_spacing_score = -1;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;
    (void)hgt;

    prompt_row = birth_prompt_row();
    body_last_row = prompt_row - 1;
    skill_rows = birth_allocation_skill_rows();
    history_lines = birth_allocation_history_lines(wid);

    if (stats_screen)
    {
        int stat_last_row = 1 + A_MAX - 1;

        for (int stat_gap = 1; stat_gap >= 0; stat_gap--)
        {
            for (int status_gap = 1; status_gap >= 0; status_gap--)
            {
                for (int history_gap = 1; history_gap >= 0; history_gap--)
                {
                    int candidate_status = stat_last_row + 1 + stat_gap;
                    int candidate_skill = candidate_status + 1 + status_gap;
                    int candidate_history = (history_lines > 0)
                        ? candidate_skill + skill_rows + history_gap : -1;
                    int candidate_room = birth_allocation_history_room(
                        candidate_history, body_last_row);
                    int spacing_score = stat_gap + status_gap + history_gap;

                    if (candidate_skill + skill_rows - 1 > body_last_row)
                        continue;

                    if (history_lines <= 0 || candidate_room >= history_lines)
                    {
                        status_row = candidate_status;
                        skill_first_row = candidate_skill;
                        history_row = candidate_history;
                        goto birth_allocation_layout_done;
                    }

                    if (candidate_room > best_history_room
                        || (candidate_room == best_history_room
                            && spacing_score > best_spacing_score))
                    {
                        best_status_row = candidate_status;
                        best_skill_first_row = candidate_skill;
                        best_history_row = candidate_history;
                        best_history_room = candidate_room;
                        best_spacing_score = spacing_score;
                    }
                }
            }
        }
    }
    else
    {
        int min_skill_first_row = BIRTH_ALLOCATION_MIN_SKILL_ROW;

        for (int candidate = skill_first_row;
             candidate >= min_skill_first_row; candidate--)
        {
            for (int status_gap = 1; status_gap >= 0; status_gap--)
            {
                for (int history_gap = 1; history_gap >= 0; history_gap--)
                {
                    int candidate_status = candidate + skill_rows + status_gap;
                    int candidate_history = (history_lines > 0)
                        ? candidate_status + 1 + history_gap : -1;
                    int candidate_room = birth_allocation_history_room(
                        candidate_history, body_last_row);
                    int spacing_score = (candidate == skill_first_row ? 2 : 1)
                        + status_gap + history_gap;

                    if (candidate_status > body_last_row)
                        continue;

                    if (history_lines <= 0 || candidate_room >= history_lines)
                    {
                        skill_first_row = candidate;
                        status_row = candidate_status;
                        history_row = candidate_history;
                        goto birth_allocation_layout_done;
                    }

                    if (candidate_room > best_history_room
                        || (candidate_room == best_history_room
                            && spacing_score > best_spacing_score))
                    {
                        best_skill_first_row = candidate;
                        best_status_row = candidate_status;
                        best_history_row = candidate_history;
                        best_history_room = candidate_room;
                        best_spacing_score = spacing_score;
                    }
                }
            }
        }
    }

    if (best_status_row >= 0)
    {
        int min_history_rows = MIN(history_lines,
            BIRTH_ALLOCATION_MIN_HISTORY_ROWS);

        skill_first_row = best_skill_first_row;
        status_row = best_status_row;
        history_row = (best_history_room >= min_history_rows)
            ? best_history_row : -1;
    }

birth_allocation_layout_done:
    if (status_row < 0)
    {
        if (stats_screen)
        {
            int stat_last_row = 1 + A_MAX - 1;

            status_row = MIN(stat_last_row + 1, body_last_row);
            skill_first_row = MIN(status_row + 1, body_last_row);
        }
        else
        {
            status_row = MIN(skill_first_row + skill_rows, body_last_row);
        }
        history_row = -1;
    }

    display_player_standard_layout_set(skill_first_row, history_row);

    if (skill_first_row_out)
        *skill_first_row_out = skill_first_row;
    if (status_row_out)
        *status_row_out = status_row;
}

static char birth_screen_char(int row, int col)
{
    unsigned char ch;

    if (!Term || !Term->scr || !Term->scr->c)
        return ' ';
    if (row < 0 || row >= Term->hgt || col < 0 || col >= Term->wid)
        return ' ';

    ch = (unsigned char)Term->scr->c[row][col];
    if (!ch || ch == (unsigned char)Term->char_blank)
        return ' ';

    return (char)ch;
}

static bool birth_screen_text_matches(int row, int col, cptr text)
{
    int len;

    if (!text)
        return false;

    len = (int)strlen(text);
    if (len <= 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        if (birth_screen_char(row, col + i) != text[i])
            return false;
    }

    return true;
}

static int birth_screen_find_text(int row, cptr text, int min_col)
{
    int wid = Term ? Term->wid : 0;
    int len;

    if (!text || !text[0] || wid <= 0)
        return -1;

    len = (int)strlen(text);
    if (len <= 0 || len > wid)
        return -1;
    if (min_col < 0)
        min_col = 0;

    for (int col = min_col; col <= wid - len; col++)
    {
        if (birth_screen_text_matches(row, col, text))
            return col;
    }

    return -1;
}

static void birth_register_visible_stat_clicks(void)
{
    int wid = 80;
    int hgt = 24;

    if (!Term || !Term->scr || !Term->scr->c)
        return;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (int row = 0; row < hgt - 1; row++)
    {
        int attr_col = birth_screen_find_text(row, "Attributes", 0);
        int skills_col;
        int start_col;
        int end_col;

        if (attr_col < 0)
            continue;

        skills_col = birth_screen_find_text(row, "Skills", attr_col + 1);
        start_col = MAX(0, attr_col - 1);
        end_col = (skills_col > attr_col) ? (skills_col - 1) : wid;
        if (end_col <= start_col)
            end_col = wid;

        for (int stat = 0; stat < A_MAX; stat++)
        {
            int stat_row = row + 1 + stat;

            if (stat_row >= hgt - 1)
                break;
            ui_menu_click_add(stat, start_col, stat_row,
                end_col - start_col);
        }

        return;
    }
}

static void birth_display_stats_allocation_compact(const int stats[A_MAX],
    int selected, int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    char buf[160];
    char stat_buf[16];
    char confirm_label[16] = "SPACE/ENTER";
    char back_label[16] = "ESC";
    char quit_label[16] = "q";
    int prompt_row;
    int info_row;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    /* Reuse compact character-sheet stats+skills page with in-place highlighted selection. */
    display_player_compact_stats_skills_highlighted_stat(selected);

    prompt_row = hgt - 1;
    if (prompt_row < 0)
        prompt_row = 0;
    info_row = prompt_row - 1;
    if (info_row < 0)
        info_row = 0;

    Term_erase(0, info_row, 255);
    Term_erase(0, prompt_row, 255);

    if (selected >= 0 && selected < A_MAX)
    {
        int cost = birth_stat_increase_cost(stats[selected]);
        cnv_stat(p_ptr->stat_use[selected], stat_buf);

        strnfmt(buf, sizeof(buf), "Selected: %s %s  Cost: %d  Left: %d",
            stat_names_full[selected], stat_buf, cost, points_left);
        c_put_str(TERM_L_BLUE, buf, info_row, 1);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Points left: %d", points_left);
        c_put_str(TERM_L_GREEN, buf, info_row, 1);
    }

    if (steamdeck)
    {
        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        strnfmt(buf, sizeof(buf), "D-pad alloc  %s back  %s ok  %s char",
            back_label, confirm_label, quit_label);
    }
    else
    {
        if (wid < 52)
            strnfmt(buf, sizeof(buf),
                "8/2 4/6  ESC back  Enter ok  q char");
        else
            strnfmt(buf, sizeof(buf),
                "8/2 select  4/6 adjust  ESC back  SPACE/ENTER ok  q char");
    }

    c_put_str(TERM_SLATE, buf, prompt_row, 1);
    birth_register_allocation_prompt_clicks(prompt_row, buf, 1,
        back_label, confirm_label, quit_label);
}

static void birth_display_skill_allocation_compact(int selected_skill, const int old_base[S_MAX],
    const int skill_gain[S_MAX], int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    char buf[160];
    char confirm_label[16] = "SPACE/ENTER";
    char back_label[16] = "ESC";
    char quit_label[16] = "q";
    int prompt_row;
    int info_row;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    /* Reuse compact character-sheet skills page with in-place highlighted selection. */
    display_player_compact_stats_skills_highlighted(selected_skill);

    prompt_row = hgt - 1;
    if (prompt_row < 0)
        prompt_row = 0;
    info_row = prompt_row - 1;
    if (info_row < 0)
        info_row = 0;

    Term_erase(0, info_row, 255);
    Term_erase(0, prompt_row, 255);

    if (selected_skill >= 0 && selected_skill < S_MAX && selected_skill != S_SPC)
    {
        int base = old_base[selected_skill];
        int gain = skill_gain[selected_skill];
        int now = base + gain;
        int cost = skill_cost(base, gain);

        strnfmt(buf, sizeof(buf), "Selected: %s %2d->%2d  Cost: %d  Left: %d",
            skill_names_full[selected_skill], base, now, cost, points_left);
        c_put_str(TERM_L_BLUE, buf, info_row, 1);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Points left: %d", points_left);
        c_put_str(TERM_L_GREEN, buf, info_row, 1);
    }

    if (steamdeck)
    {
        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        strnfmt(buf, sizeof(buf), "D-pad alloc  %s back  %s ok  %s char",
            back_label, confirm_label, quit_label);
    }
    else
    {
        if (wid < 52)
            strnfmt(buf, sizeof(buf),
                "8/2 4/6  ESC back  Enter ok  q char");
        else
            strnfmt(buf, sizeof(buf),
                "8/2 select  4/6 adjust  ESC back  SPACE/ENTER ok  q char");
    }

    c_put_str(TERM_SLATE, buf, prompt_row, 1);
    birth_register_allocation_prompt_clicks(prompt_row, buf, 1,
        back_label, confirm_label, quit_label);
}

/*
 * Helper function for 'player_birth()'.
 */
static NavResult player_birth_aux_2(int stats[A_MAX])
{
    int i;

    int row = 1;
    int col = 43;

    int stat = 0;

    int cost;

    char ch;

    char buf[80];
    NavResult result = NAV_BACK;

    /* Determine experience and things */
    get_extra();

    /* Show tutorial for first-time players (when scorefile is empty) */
    /* Do this AFTER get_extra() so character has stats/abilities to display */
    /* Check every time - show tutorial for every new character if scores file is empty */
    log_debug("Checking if tutorial should be shown...");
    bool is_empty = highscore_is_empty();
    log_debug("highscore_is_empty() returned: %s", is_empty ? "true" : "false");
    if (!run_mode_is_blitz() && is_empty)
    {
        log_info("First-time player detected - showing character screen tutorial");
        
        /* Initialize character stats for display - same as first iteration of stats loop */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain bonuses for race/character */
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);
            
            /* Set base stats (0 + racial/character bonuses) */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;
        }
        
        /* Calculate bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);
        update_stuff();
        
        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;
        
        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;
        
        /* Now show the tutorial with a realistic character sheet */
        display_character_tutorial();
        log_info("Character screen tutorial completed");
    }
    else
    {
        log_info("Not showing tutorial - scores file has entries");
    }

    log_trace("Starting stats allocation interface");
    screen_push_touch_pane_proto();

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();
        int wid = 80;
        int hgt = 24;
        Term_get_size(&wid, &hgt);
        if (wid < 1) wid = 80;
        if (hgt < 1) hgt = 24;
        bool compact = (wid < 80);
        int wide_offset = (wid > 80) ? (wid - 80) / 2 : 0;
        int sheet_col = col + wide_offset;

        /* Reset cost */
        cost = 0;

        /* Process stats */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain a "bonus" for "race" */
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);

            /* Apply the racial bonuses */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;

            /* Total cost */
            cost += birth_stat_costs[stats[i] + 4];
        }

        /* Restrict cost */
        if (cost > MAX_COST)
        {
            /* Warning */
            bell("Excessive stats!");

            /* Reduce stat */
            stats[stat]--;

            /* Recompute costs */
            continue;
        }

        p_ptr->new_exp = p_ptr->exp = get_start_xp();

        /* Calculate the bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);

        /* Update stuff */
        update_stuff();

        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;

        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        if (compact)
        {
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            birth_display_stats_allocation_compact(stats, stat, MAX_COST - cost, steamdeck);
            birth_register_visible_stat_clicks();
            ui_scroll_area_begin(0, hgt - 2, SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');
        }
        else
        {
            int prompt_row = birth_prompt_row();
            int status_row = row + A_MAX;
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            ui_scroll_area_begin(row, row + A_MAX - 1,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');

            /* Display the player */
            birth_configure_allocation_sheet_layout(true, NULL, &status_row);
            display_player(0);
            display_player_standard_layout_clear();

            /* Display the costs header */
            c_put_str(TERM_WHITE, "Points Left:", 0, sheet_col + 21);
            strnfmt(buf, sizeof(buf), "%2d", MAX_COST - cost);
            c_put_str(TERM_L_GREEN, buf, 0, sheet_col + 34);

            /* Display the costs */
            for (i = 0; i < A_MAX; i++)
            {
                if (i == stat)
                {
                    byte attr = TERM_L_BLUE;

                    /* Match the character sheet label rendering: trim trailing spaces.
                     * (stat_names[] include padding for mono layouts.) */
                    const char* stat_label = (p_ptr->stat_drain[i] < 0) ? stat_names_reduced[i] : stat_names[i];
                    char trimmed_label[32];
                    SDL_strlcpy(trimmed_label, stat_label ? stat_label : "", sizeof(trimmed_label));
                    int len = (int)strlen(trimmed_label);
                    while (len > 0 && trimmed_label[len - 1] == ' ') {
                        trimmed_label[--len] = '\0';
                    }

                    bool use_story = story_character_enabled();
                    if (use_story) {
                        sdl_story_font_enable();
                    }

                    c_put_str(attr, trimmed_label, row + i, sheet_col - 1);

                    if (use_story) {
                        sdl_story_font_disable();
                    }

#ifndef MONOCHROME_MODE
                    strnfmt(buf, sizeof(buf), "%4d", birth_stat_increase_cost(stats[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 32);
#else
                    strnfmt(buf, sizeof(buf), "%4d*", birth_stat_increase_cost(stats[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 32);
                    c_put_str(attr, "*", row + i, sheet_col - 2);
#endif
                }
                else
                {
                    byte attr = TERM_L_WHITE;
                    strnfmt(buf, sizeof(buf), "%4d", birth_stat_increase_cost(stats[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 32);
                }
                ui_menu_click_add(i, sheet_col - 2, row + i, 40);
            }

            if (status_row < prompt_row)
            {
                char stat_buf[80];
                char stat_label[16];
                int stat_label_len;

                SDL_strlcpy(stat_label, stat_names[stat], sizeof(stat_label));
                stat_label_len = (int)strlen(stat_label);
                while (stat_label_len > 0
                    && stat_label[stat_label_len - 1] == ' ')
                {
                    stat_label[--stat_label_len] = '\0';
                }

                strnfmt(stat_buf, sizeof(stat_buf),
                    "%s %d Cost:%d Left:%d",
                    stat_label, p_ptr->stat_use[stat],
                    birth_stat_increase_cost(stats[stat]), MAX_COST - cost);
                birth_draw_allocation_confirm_status(status_row, sheet_col - 1,
                    sheet_col + 37, stat_buf);
            }

            /* Bottom bar follows character sheet font setting */
            if (story_character_enabled()) {
                sdl_story_font_enable();
            }

            if (steamdeck) {
                char confirm_label[16];
                char back_label[16];
                char quit_label[16];
                char prompt_buf[160];

                /* Steam Deck UI: A=confirm, B=back, q=character selection */
                birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
                birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "D-pad allocate  %s back  %s confirm  %s char",
                    back_label, confirm_label, quit_label);
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt_buf);
                birth_register_allocation_prompt_clicks(prompt_row,
                    prompt_buf, QUESTION_COL, back_label, confirm_label,
                    quit_label);
            } else {
                cptr prompt_text =
                    "Arrows -allocate    ESC -back   SPACE/ENTER -confirm   q -character";
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE,
                    prompt_text);
                birth_register_allocation_prompt_clicks(prompt_row,
                    prompt_text, QUESTION_COL, "ESC", "SPACE/ENTER", "q");
            }

            if (story_character_enabled()) {
                sdl_story_font_disable();
            }
        }

        /* Get key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < A_MAX)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != stat)
                    {
                        stat = clicked_choice;
                        continue;
                    }
                    ch = (click_action == UI_MENU_CLICK_SECONDARY) ? '4' : '6';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    ch = ESCAPE;
                else if (clicked_choice == -2)
                    ch = '\r';
                else if (clicked_choice == -3)
                    ch = 'q';
            }
        }

        /* Return to character selection before the game starts */
        if ((ch == 'Q') || (ch == 'q')) {
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = (turn == 0) ? NAV_BACK : NAV_QUIT;
            goto cleanup;
        }

        /* Back to Character Selection */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_BACK;
            goto cleanup;
        }

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
        {
            if (!birth_confirm_unspent_stat_points(MAX_COST - cost, steamdeck))
                continue;
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_OK;
            goto cleanup;
        }

        /* Prev stat */
        if (ch == '8')
        {
            stat = (stat + A_MAX - 1) % A_MAX;
        }

        /* Next stat */
        if (ch == '2')
        {
            stat = (stat + 1) % A_MAX;
        }

        /* Decrease stat */
        if ((ch == '4') && (stats[stat] > 0))
        {
            stats[stat]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            stats[stat]++;
        }
    }

    /* Shouldn't reach; default to back */
cleanup:
    screen_pop_touch_pane_proto();
    ui_scroll_area_clear();
    return result;
}

/*
 * Skill point costs.
 *
 * The nth skill point costs (100*n) experience points
 */
static int skill_cost(int base, int points)
{
    int total_cost = (points + base) * (points + base + 1) / 2;
    int prev_cost = (base) * (base + 1) / 2;
    return ((total_cost - prev_cost) * 100);
}

static int gain_skills_initial_skill = -1;

void gain_skills_set_initial_skill(int skill)
{
    if (skill < 0 || skill >= S_MAX || skill == S_SPC)
        gain_skills_initial_skill = -1;
    else
        gain_skills_initial_skill = skill;
}

static char gain_skills_screen_char(int row, int col)
{
    unsigned char ch;

    if (!Term || !Term->scr || !Term->scr->c)
        return ' ';
    if (row < 0 || row >= Term->hgt || col < 0 || col >= Term->wid)
        return ' ';

    ch = (unsigned char)Term->scr->c[row][col];
    if (!ch || ch == (unsigned char)Term->char_blank)
        return ' ';

    return (char)ch;
}

static bool gain_skills_screen_text_matches(int row, int col, cptr text,
    int len)
{
    if (!text || len <= 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        if (gain_skills_screen_char(row, col + i) != text[i])
            return false;
    }

    return true;
}

static bool gain_skills_screen_row_has_value(int row, int start_col)
{
    int wid = Term ? Term->wid : 0;

    for (int col = start_col; col < wid; col++)
    {
        if (gain_skills_screen_char(row, col) == '=')
            return true;
    }

    return false;
}

static void gain_skills_register_visible_skill_clicks(void)
{
    int wid = 80;
    int hgt = 24;

    if (!Term || !Term->scr || !Term->scr->c)
        return;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (int skill = 0; skill < S_MAX; skill++)
    {
        cptr name;
        int name_len;
        int match_len;
        bool found = false;

        if (skill == S_SPC)
            continue;

        name = skill_names_full[skill];
        if (!name || !name[0])
            continue;

        name_len = (int)strlen(name);
        match_len = name_len;
        if (match_len > 5)
            match_len = 5;
        if (match_len < 4)
            match_len = name_len;

        for (int row = 0; row < hgt - 1 && !found; row++)
        {
            for (int col = 0; col <= wid - match_len; col++)
            {
                int start_col;

                if (!gain_skills_screen_text_matches(row, col, name,
                    name_len)
                    && !gain_skills_screen_text_matches(row, col, name,
                        match_len))
                {
                    continue;
                }

                if (!gain_skills_screen_row_has_value(row, col + match_len))
                    continue;

                start_col = MAX(0, col - 1);
                ui_menu_click_add(skill, start_col, row, wid - start_col);
                found = true;
                break;
            }
        }
    }
}

/*
 * Increase your skills by spending experience points
 */
extern NavResult gain_skills(void)
{
    int i;

    int row = 6;
    int col = 43;

    int skill = ((gain_skills_initial_skill >= 0
        && gain_skills_initial_skill < S_MAX
        && gain_skills_initial_skill != S_SPC)
        ? gain_skills_initial_skill
        : 0);

    int old_base[S_MAX];
    int skill_gain[S_MAX];

    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;

    char ch;

    char buf[80];

    NavResult result = NAV_OK;

    int tab = 0;
    bool force_initial_redraw = true;

    log_debug("Starting skills allocation with %d experience points", p_ptr->new_exp);
    gain_skills_initial_skill = -1;

    // hack global variable
    skill_gain_in_progress = true;

    /* save the old skills */
    for (i = 0; i < S_MAX; i++)
        old_base[i] = p_ptr->skill_base[i];

    /* initialise the skill gains */
    for (i = 0; i < S_MAX; i++)
        skill_gain[i] = 0;

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();

        /* Recompute points/costs and apply the temporary skill increases */
        total_cost = 0;

        for (i = 0; i < S_MAX; i++)
        {
            /* Skip Special abilities skill - not trainable */
            if (i == S_SPC) continue;
            total_cost += skill_cost(old_base[i], skill_gain[i]);
        }

        p_ptr->new_exp = old_new_exp - total_cost;

        if (p_ptr->new_exp < 0)
        {
            bell("Excessive skills!");
            skill_gain[skill]--;
            continue;
        }

        p_ptr->update |= (PU_BONUS);
        p_ptr->redraw |= (PR_EXP | PR_BASIC);

        for (i = 0; i < S_MAX; i++)
        {
            if (i == S_SPC) continue;
            p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
        }

        update_stuff();

        int wid = 80;
        int hgt = 24;
        Term_get_size(&wid, &hgt);
        if (wid < 1) wid = 80;
        if (hgt < 1) hgt = 24;
        bool compact = (wid < 80);
        int wide_offset = (wid > 80) ? (wid - 80) / 2 : 0;
        int sheet_col = col + wide_offset;

        if (compact)
        {
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            birth_display_skill_allocation_compact(skill, old_base, skill_gain, p_ptr->new_exp, steamdeck);
            gain_skills_register_visible_skill_clicks();
            ui_scroll_area_begin(0, hgt - 2, SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');
        }
        else
        {
            int prompt_row = birth_prompt_row();
            int status_row = row + S_SNG + 1;
            int skill_first_row = row;
            ui_menu_click_begin();
            ui_menu_click_set_hover_enabled(true);
            birth_configure_allocation_sheet_layout(false, &skill_first_row,
                &status_row);
            row = skill_first_row;
            ui_scroll_area_begin(row, row + S_SNG,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('6', '4', '6', '4');

            /* Display the player */
            display_player(0);
            display_player_standard_layout_clear();

            /* Display the costs header */
            if (!character_dungeon)
            {
                if (p_ptr->new_exp >= 10000)
                    tab = 0;
                else if (p_ptr->new_exp >= 1000)
                    tab = 1;
                else if (p_ptr->new_exp >= 100)
                    tab = 2;
                else if (p_ptr->new_exp >= 10)
                    tab = 3;
                else
                    tab = 4;

                strnfmt(buf, sizeof(buf), "%6d", p_ptr->new_exp);
                c_put_str(TERM_L_GREEN, buf, row - 2, sheet_col + 30);
                c_put_str(TERM_WHITE, "Points Left:", row - 2, sheet_col + 17 + tab);
            }

            /* Display the costs */
            for (i = 0; i < S_MAX; i++)
            {
                /* Skip Special abilities skill - not trainable */
                if (i == S_SPC) continue;

                if (i == skill)
                {
                    byte attr = TERM_L_BLUE;

                    bool use_story = story_character_enabled();
                    if (use_story) {
                        sdl_story_font_enable();
                    }

                    c_put_str(attr, skill_names_full[i], row + i, sheet_col - 1);

                    if (use_story) {
                        sdl_story_font_disable();
                    }

#ifndef MONOCHROME_MODE
                    strnfmt(buf, sizeof(buf), "%6d",
                        skill_cost(old_base[i], skill_gain[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 30);
#else
                    strnfmt(buf, sizeof(buf), "%6d*",
                        skill_cost(old_base[i], skill_gain[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 30);
                    c_put_str(attr, "*", row + i, sheet_col - 2);
#endif
                }
                else
                {
                    byte attr = TERM_L_WHITE;
                    strnfmt(buf, sizeof(buf), "%6d",
                        skill_cost(old_base[i], skill_gain[i]));
                    c_put_str(attr, buf, row + i, sheet_col + 30);
                }
                ui_menu_click_add(i, sheet_col - 2, row + i, 40);
            }

            if (status_row < prompt_row)
            {
                char skill_buf[80];

                strnfmt(skill_buf, sizeof(skill_buf), "Cost:%d Left:%d",
                    skill_cost(old_base[skill], skill_gain[skill]),
                    p_ptr->new_exp);
                birth_draw_allocation_confirm_status(status_row, sheet_col - 1,
                    sheet_col + 37, skill_buf);
            }

            /* Bottom bar follows character sheet font setting */
            if (story_character_enabled()) {
                sdl_story_font_enable();
            }

            if (steamdeck) {
                char confirm_label[16];
                char back_label[16];
                char quit_label[16];
                char prompt_buf[160];

                /* Steam Deck UI: A=confirm, B=back, q=character selection */
                birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
                birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "D-pad -allocate      %s-back     %s-confirm     %s-char",
                    back_label, confirm_label, quit_label);
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt_buf);
                birth_register_allocation_prompt_clicks(prompt_row,
                    prompt_buf, QUESTION_COL, back_label, confirm_label,
                    quit_label);
            } else {
                cptr prompt_text =
                    "Arrows -allocate      ESC -back     SPACE/ENTER -confirm     q -character";
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE,
                    prompt_text);
                birth_register_allocation_prompt_clicks(prompt_row,
                    prompt_text, QUESTION_COL, "ESC", "SPACE/ENTER", "q");
            }

            if (story_character_enabled()) {
                sdl_story_font_disable();
            }
        }

        if (force_initial_redraw)
        {
            Term_redraw();
            force_initial_redraw = false;
        }

        /* Get key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < S_MAX
                    && clicked_choice != S_SPC)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != skill)
                    {
                        skill = clicked_choice;
                        continue;
                    }
                    ch = (click_action == UI_MENU_CLICK_SECONDARY) ? '4' : '6';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    ch = ESCAPE;
                else if (clicked_choice == -2)
                    ch = '\r';
                else if (clicked_choice == -3)
                    ch = 'q';
            }
        }

        /* Return to character selection before the game starts */
        if (((ch == 'Q') || (ch == 'q')) && (turn == 0)) {
            /* restore state before leaving */
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            skill_gain_in_progress = false;
            ui_menu_click_clear();
            ui_scroll_area_clear();
            return NAV_TO_CHARACTER;
        }

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
        {
            if (compact && birth_pending_compact_description_confirm)
            {
                if (!birth_show_compact_description_after_assignment(steamdeck))
                    continue;
            birth_pending_compact_description_confirm = false;
            }
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_OK;
            break;
        }

        /* Abort */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            ui_menu_click_clear();
            ui_scroll_area_clear();
            result = NAV_BACK;   /* go back to stat allocation */
            break;
        }

        /* Prev skill */
        if (ch == '8')
        {
            do {
                skill = (skill + S_MAX - 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Next skill */
        if (ch == '2')
        {
            do {
                skill = (skill + 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Decrease skill */
        if ((ch == '4') && (skill_gain[skill] > 0))
        {
            skill_gain[skill]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            /* Don't allow increasing Special abilities skill */
            if (skill != S_SPC) {
                skill_gain[skill]++;
            }
        }
    }

    // reset hack global variable
    ui_menu_click_clear();
    ui_scroll_area_clear();
    skill_gain_in_progress = false;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update stuff */
    update_stuff();

    log_debug("Skills allocation completed, spent %d experience", old_new_exp - p_ptr->new_exp);

    /* Done */
    return result;
}

#define BASE_COLUMN 7
#define STAT_TITLE_ROW 14
#define BASE_STAT_ROW 16

/*
 * Helper function for 'player_birth()'.
 *
 * See "display_player" for screen layout code.
 */
static NavResult player_birth_aux(void)
{

    log_debug("Initializing character data and history");
    birth_pending_compact_description_confirm = true;

    SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(op_ptr->full_name));
    process_player_name(true);  /* CRITICAL: Must pass true to update savefile path! */
    /* Clear the previous history strings */
    p_ptr->history[0] = '\0';
    SDL_strlcat(
                p_ptr->history, (c_text + c_info[p_ptr->pcharacter].text), sizeof(p_ptr->history));

    p_ptr->wt = 0;
    p_ptr->ht = 0;
    p_ptr->age = 0;

    /* Oath selection (after character creation, before tutorial/stats) */
    if (run_mode_is_blitz() && !blitz_oaths_enabled())
    {
        p_ptr->oath_type = 0;
    }
    else
    {
        log_debug("Entering oath selection");
        NavResult oath_result = select_oath();
        if (oath_result != NAV_OK) return oath_result;
        log_debug("Oath selection completed");
    }

    if (run_mode_is_blitz())
    {
        NavResult blitz_effects = blitz_configure_effects();
        if (blitz_effects != NAV_OK)
            return blitz_effects;
    }

    /* Point-based flow */
    if (blitz_auto_allocates_stats())
    {
        NavResult auto_result = blitz_auto_build_character();
        if (auto_result != NAV_OK)
            return auto_result;
    }
    else
    {
        int stat_alloc[A_MAX];

        for (int i = 0; i < A_MAX; i++)
            stat_alloc[i] = p_ptr->stat_base[i];

        for (;;)
        {
            display_player(0);

            /* Stats allocation screen */
            log_debug("Entering stats allocation");
            NavResult s = player_birth_aux_2(stat_alloc);
            if (s == NAV_OK) {
                /* Skill allocation: Esc returns to stats; q returns to character selection. */
                log_debug("Stats accepted, entering skills allocation");
                screen_push_touch_pane_proto();
                NavResult g = gain_skills();
                screen_pop_touch_pane_proto();
                if (g == NAV_BACK) continue;
                if (g == NAV_TO_CHARACTER) return NAV_BACK;
                if (g != NAV_OK) return g;
                log_debug("Skills allocation completed");
                break; /* accepted */
            }
            if (s == NAV_BACK)   return NAV_BACK;    /* back to Character Selection */
            if (s == NAV_TO_CHARACTER) return NAV_BACK; /* back to Character Selection */
            if (s == NAV_TO_MAIN) return NAV_TO_MAIN;/* back to main menu */
            if (s == NAV_QUIT)   return NAV_QUIT;    /* hard exit */
            /* any other value: loop again */
        }
    }

    // Reset the number of artefacts
    p_ptr->artefacts = 0;

    log_trace("Final character stats: Str=%d Dex=%d Con=%d Gra=%d",
              p_ptr->stat_base[A_STR], p_ptr->stat_base[A_DEX],
              p_ptr->stat_base[A_CON], p_ptr->stat_base[A_GRA]);

    /* Accept */
    return NAV_OK;
}

/*
 * Create a new character.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
NavResult player_birth()
{
    int i;

    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    log_info("Starting character creation process");
    killer_reset();

    /* Create a new character */
    while (1)
    {
        NavResult r = player_birth_aux();
        if (r == NAV_OK) break;
        if (r == NAV_BACK) return NAV_BACK;         /* back to character_selection */
        if (r == NAV_TO_CHARACTER) return NAV_BACK; /* back to character_selection */
        if (r == NAV_TO_MAIN) return NAV_TO_MAIN;   /* back to main menu */
        if (r == NAV_QUIT) return NAV_QUIT;         /* hard exit */
        /* Any other value -> retry loop */
    }

    for (i = 0; i < NOTES_LENGTH; i++)
    {
        notes_buffer[i] = '\0';
    }

    /* Get date */
    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    sprintf(month, "%.2s", raw_date + 5);
    atomonth(atoi(month), month);

    if (*(raw_date + 7) == '0')
        sprintf(
            clean_date, "%.1s %.3s %.4s", raw_date + 8, month, raw_date + 1);
    else
        sprintf(
            clean_date, "%.2s %.3s %.4s", raw_date + 7, month, raw_date + 1);

    /* Add in "character start" information */
    SDL_strlcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    SDL_strlcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    SDL_strlcat(
        notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));

    /* Note player birth in the message recall */
    message_add(" ", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add("====================", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add(" ", MSG_GENERIC);

    /* Hack -- outfit the player */
    player_outfit();

    /* Load persistent settings from metarun if this is a continuing metarun */
    if (!run_mode_is_blitz())
        metarun_load_persistent_settings();

    /* Reapply app-wide settings after character creation so UI preferences are
     * not sourced from the metarun or savefile. */
    sdl_config_load_app_options(get_sdl_config_path());

    log_info("Character creation completed: %s the %s", op_ptr->full_name, p_name + rp_ptr->name);

    return NAV_OK;
}














