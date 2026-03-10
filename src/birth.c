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
#include "fs/path.h"
#include "log/log.h"
#include "player/killer.h"
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

        /* Light sources start with fuel */
        if (slot == INVEN_LITE) i_ptr->timeout = 2000;

        bool start_known = true;
        if ((i_ptr->tval == TV_POTION)
            || (i_ptr->tval == TV_FOOD && i_ptr->sval <= SV_FOOD_SICKNESS)
            || (i_ptr->tval == TV_GEM))
        {
            if (!player_auto_identifies_object(i_ptr))
                start_known = false;
        }

        if (start_known)
            object_known(i_ptr);

        /* Carry it */
        int carry_slot = inven_carry(i_ptr, true);

        if (carry_slot == SUPPLIES_INDEX)
        {
            object_type copy;
            object_copy(&copy, i_ptr);
            char name[80];
            object_desc(name, sizeof(name), &copy, true, 3);
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
        
        /* Convert artifact name to lowercase */
        for (int j = 0; a_ptr->name[j] && j < MAX_LEN_ART_NAME - 1; j++) {
            art_lower[j] = tolower((unsigned char)a_ptr->name[j]);
        }
        art_lower[strlen(a_ptr->name)] = '\0';
        
        /* Check if artifact name contains "of {CharacterName}" */
        if (strstr(art_lower, pattern_lower)) {
            /* Verify it's a valid base kind */
            int k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (k_idx) {
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
        }
    }
    
    /* If no named artifact, use the original random selection logic */
    if (art_idx == 0) {
        int candidates[512];
        int candidate_kinds[512];
        int count = 0;
        int max_level = 10;

        for (int pass = 0; pass < 2; pass++) {
            count = 0;
            for (int i = 1; i < z_info->art_max && count < (int)N_ELEMENTS(candidates); i++) {
                artefact_type *a_ptr = &a_info[i];
                if (!a_ptr->name[0]) continue;
                if (a_ptr->cur_num > 0) continue;
                if (a_ptr->level > max_level) continue;
                if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;
                int k = lookup_kind(a_ptr->tval, a_ptr->sval);
                if (!k) continue;
                candidates[count] = i;
                candidate_kinds[count] = k;
                count++;
            }

            if (count > 0)
                break;

            max_level = MORGOTH_DEPTH;
        }

        if (count == 0) {
            log_warn("No artefacts available for starting blessing.");
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

    /* skip all starting‐gear on load */
    if (character_loaded) return;

    /* ---------- escape-curse check ---------- */
    if (curse_flag_count_cur(CUR_NOSTART)) return;

    /* ---------- pointers into info arrays ---------- */
    player_race  *rp_ptr = &p_info[p_ptr->prace];
    character_profile *current_character_profile = &c_info[p_ptr->pcharacter];

    /* ---------- hand out gear ---------- */
    log_debug("Giving starting items for race: %s", p_name + rp_ptr->name);
    give_start_items(rp_ptr->start_items);   /* race first  */
    log_debug("Giving starting items for character: %s", c_name + current_character_profile->name);
    give_start_items(current_character_profile->start_items);   /* character kit */

    if (metarun_has_major_blessing_effect(METARUN_MAJOR_EFFECT_START_ARTIFACT)) {
        grant_starting_artifact();
    }

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

static bool character_flags_need_compact_layout(void)
{
    int wid = 80;
    int hgt = 24;
    int min_flags_wid = TOTAL_AUX_COL + 21 + 20;

    Term_get_size(&wid, &hgt);
    (void)hgt;
    if (wid < 1)
        wid = 80;

    return (wid < min_flags_wid);
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

    min_row = TABLE_ROW + visible_rows + 1;
    row = MAX(min_row, row);

    if (row > birth_prompt_row() - 1)
        row = birth_prompt_row() - 1;
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

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

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

    screen_save();
    Term_clear();

    Term_putstr(2, 0, -1, TERM_L_BLUE, full_name);

    if (choice.text && choice.text[0] && (hgt > 2))
    {
        text_out_wrap = wid - 1;
        text_out_indent = 2;
        Term_gotoxy(2, 2);
        text_out_to_screen(TERM_WHITE, choice.text);
        text_out_wrap = 0;
        text_out_indent = 0;
    }

    if (hgt > 0)
        Term_putstr(2, hgt - 1, -1, TERM_SLATE, "Press any key to return");

    Term_fresh();
    (void)inkey();

    screen_load();
}

/*
 * Generic "get choice from menu" function
 */
static int get_player_choice(birth_menu* choices, int num, int def, int col,
    int wid, void (*hook)(birth_menu), bool allow_full_description_screen)
{
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
    int clear_limit = birth_prompt_row() + 1;

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

        hgt = birth_prompt_row() - TABLE_ROW - 1;

        /*
         * If we're going to use a tighter, compact traits layout on very short
         * screens, reserve one extra line above DESCRIPTION_ROW.
         *
         * This avoids the menu list occupying the same row we may use for
         * compact trait output.
         */
        if (allow_full_description_screen)
        {
            bool compact_flags = character_flags_need_compact_layout();
            bool very_short = (Term->hgt > 0) && (Term->hgt <= 20);
            if (compact_flags && very_short)
            {
                int max_hgt = (description_row - 2) - TABLE_ROW;
                if (max_hgt < 0) max_hgt = 0;
                if (hgt > max_hgt) hgt = max_hgt;
            }
        }

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
        }

        list_rows_drawn = i;

        if (!allow_full_description_screen)
            description_row = choice_description_row(list_rows_drawn, false);

        {
            int clear_from_row = description_row;
            if (allow_full_description_screen)
            {
                bool compact_flags = character_flags_need_compact_layout();
                bool very_short = (Term->hgt > 0) && (Term->hgt <= 20);
                if (compact_flags && very_short)
                    clear_from_row = description_row - 1;
            }
            for (i = clear_from_row; i < Term->hgt; i++)
                Term_erase(0, i, 255);
        }

        compact_flags = (allow_full_description_screen && character_flags_need_compact_layout());
        show_description = (!compact_flags);

        if (allow_full_description_screen)
            show_description = (show_description && character_description_has_room());

        if (show_description && choices[cur].text != NULL)
        {
            /* Indent output by 2 character, and wrap at column 79 */
            text_out_wrap = 79;
            text_out_indent = 2;

            /* History */
            Term_gotoxy(text_out_indent, description_row);
            text_out_to_screen(TERM_WHITE, choices[cur].text);

            /* Reset text_out() vars */
            text_out_wrap = 0;
            text_out_indent = 0;
        }

        if (done)
            return (cur);

        /* Display auxiliary information if any is available. */
        if (hook)
            hook(choices[cur]);

        if (Term->hgt > 0)
        {
            prompt_row = birth_prompt_row();
            Term_erase(0, prompt_row, 255);

            if (allow_full_description_screen)
                strnfmt(prompt, sizeof(prompt), "ENTER select  f description  ESC back  r random");
            else
                strnfmt(prompt, sizeof(prompt), "ENTER select  ESC back  r random");

            (void)steamdeck;
            Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt);
        }

        /* Move the cursor */
        put_str("", TABLE_ROW + cur - top, col);

        hide_cursor = true;
        c = inkey();
        hide_cursor = false;

        /* Exit the game */
        if ((c == 'Q') || (c == 'q'))
            quit(NULL);

        /* Hack - go back */
        if ((c == ESCAPE) || (c == '4') || (steamdeck && c == 'b'))
            return (INVALID_CHOICE);

        /* Make a choice */
        if ((c == '\n') || (c == '\r') || (c == ' ') || (c == '6')) {
            if (choices[cur].ghost)
                bell("Your race cannot choose that character.");
            else
                return (cur);
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

        if (allow_full_description_screen && (c == 'f' || c == 'F'))
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
    else if (isalpha(c))
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


/*
 * Show race/character flags in priority order.
 * Masteries first, then single-side affinities, then penalties,
 * and finally any “headline / unique” flags.
 */
static void print_rh_flags(int race, int character, int col, int row)
{
    int flags_left  = 0;
    int flags_right = 0;
    bool compact_layout = character_flags_need_compact_layout();
    int description_row = birth_description_base_row();
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

    skill_line mastery_buf [16], affinity_buf[16], penalty_buf[16], unique_buf[8];
    int mastery_n = 0, affinity_n = 0, penalty_n = 0, unique_n = 0;

/*
 * Show one skill line according to the new ±2 ↔ mastery / grand-penalty rule.
 *
 *   +1 for every …_AFFINITY bit, −1 for every …_PENALTY bit.
 *
 *        score   meaning            colour / buffer
 *        =====   ===============    =========================
 *          +2    mastery            mastery_buf  / attr_mastery
 *          +1    affinity           affinity_buf / attr_affinity
 *           0    (omit line)        —
 *          −1    penalty            penalty_buf  / attr_penalty
 *          −2    grand penalty      penalty_buf  / attr_penalty
 */
/* Show one skill line according to the new ±2 rule,
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
        int compact_row = description_row;
        int compact_col = 2;
        int col_gap = 2;
        int col_wid;
        bool use_two_columns = false;
        int right_offset = 0; /* 0=normal, -1=right column starts on title row, -2=one row above */
        char line_buf[64];

        typedef struct {
            cptr txt;
            byte attr;
        } compact_flag_line;

        compact_flag_line compact_lines[64];
        int compact_line_n = 0;
        int max_line_len = 0;

#define APPEND_COMPACT_LINE(text_value, color_value)                       \
    do {                                                                    \
        if ((text_value) && compact_line_n < (int)N_ELEMENTS(compact_lines)) \
        {                                                                   \
            compact_lines[compact_line_n].txt = (text_value);              \
            compact_lines[compact_line_n].attr = (color_value);            \
            if ((int)strlen(text_value) > max_line_len)                    \
                max_line_len = (int)strlen(text_value);                    \
            compact_line_n++;                                               \
        }                                                                   \
    } while (0)

        for (int i = 0; i < ability_line_n && (row + i) < description_row; ++i)
            Term_putstr(col, row + i, -1, TERM_YELLOW, ability_lines[i]);

        for (int i = 0; i < unique_n; ++i)
            if (unique_buf[i].side == 0)
                APPEND_COMPACT_LINE(unique_buf[i].txt, unique_buf[i].col);
        for (int i = 0; i < mastery_n; ++i)
            APPEND_COMPACT_LINE(mastery_buf[i].txt, mastery_buf[i].col);
        for (int i = 0; i < affinity_n; ++i)
            APPEND_COMPACT_LINE(affinity_buf[i].txt, affinity_buf[i].col);
        for (int i = 0; i < unique_n; ++i)
            if (unique_buf[i].side == 1)
                APPEND_COMPACT_LINE(unique_buf[i].txt, unique_buf[i].col);
        for (int i = 0; i < penalty_n; ++i)
            APPEND_COMPACT_LINE(penalty_buf[i].txt, penalty_buf[i].col);

        col_wid = (Term->wid - compact_col - col_gap) / 2;
        if (col_wid < 1)
            col_wid = 1;

        /* Default: only use two columns if everything fits without truncation. */
        if (col_wid >= max_line_len)
            use_two_columns = true;

        /*
         * For short screens (<24 rows), try to guarantee at least 10 visible
         * traits (or all traits if fewer than 10), but only apply compaction
         * techniques if they are needed.
         */
        {
            const bool short_screen = (Term->hgt > 0) && (Term->hgt < 24);
            const int target_traits = (compact_line_n < 10) ? compact_line_n : 10;
            const int min_col_wid_for_forced_two_cols = 14;

#define MAX0(v) ((v) > 0 ? (v) : 0)
#define CAPACITY_ONE(_row) (MAX0(Term->hgt - ((_row) + 1) - 1))
#define CAPACITY_TWO(_row, _roff) \
    (MAX0(Term->hgt - ((_row) + 1) - 1) + MAX0(Term->hgt - ((_row) + 1 + (_roff)) - 1))

            /* Helper: compute how many traits can be shown with given settings. */
#define TRAIT_CAPACITY(_row, _pack_title, _two_cols) \
    (((Term->hgt - (_row) - 1) <= 0) ? 0 : \
      (((_pack_title) ? (Term->hgt - (_row) - 1) : (Term->hgt - (_row) - 2)) <= 0 ? 0 : \
        (((_pack_title) ? (Term->hgt - (_row) - 1) : (Term->hgt - (_row) - 2)) * ((_two_cols) ? 2 : 1))))

            int base_capacity = use_two_columns ? CAPACITY_TWO(compact_row, right_offset)
                                               : CAPACITY_ONE(compact_row);

            if (short_screen && (base_capacity < target_traits))
            {
                /* Step 1: move the "Character traits" title up by 1 row if needed. */
                if (compact_row > 0)
                    compact_row = description_row - 1;

                base_capacity = use_two_columns ? CAPACITY_TWO(compact_row, right_offset)
                                               : CAPACITY_ONE(compact_row);

                /* Step 2: if needed, allow two columns (may truncate) to reach the target. */
                if ((base_capacity < target_traits) && !use_two_columns
                    && (col_wid >= min_col_wid_for_forced_two_cols))
                {
                    use_two_columns = true;
                    base_capacity = CAPACITY_TWO(compact_row, right_offset);
                }

                /* Step 3: start the *second* column on the title row. */
                if ((base_capacity < target_traits) && use_two_columns)
                {
                    right_offset = -1;
                    base_capacity = CAPACITY_TWO(compact_row, right_offset);
                }

                /* Step 4 (last resort): start the second column one row above the title. */
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
                if (left_rows < 0) left_rows = 0;
                if (right_rows < 0) right_rows = 0;

                int max_lines = left_rows + right_rows;
                int draw_lines = (compact_line_n < max_lines) ? compact_line_n : max_lines;

                int left_count = (draw_lines + 1) / 2;
                if (left_count > left_rows) left_count = left_rows;
                int right_count = draw_lines - left_count;
                if (right_count > right_rows) right_count = right_rows;
                if (left_count > left_rows) left_count = left_rows;
                if (left_count > (draw_lines - right_count)) left_count = draw_lines - right_count;
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
                if (rows < 0) rows = 0;

                int draw_lines = (compact_line_n < rows) ? compact_line_n : rows;
                for (int i = 0; i < draw_lines; ++i)
                {
                    Term_putstr(compact_col, start_row + i, -1,
                        compact_lines[i].attr, compact_lines[i].txt);
                }
            }
        }

#undef APPEND_COMPACT_LINE
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
    
    print_rh_flags(
        p_ptr->prace, character_idx, TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1);
    
    /* Display power rating legend on left side at row 10 with alive counts */
    int legend_col = 2;  /* Left side */
    int legend_row = 10; /* Row 10 as requested (moved up one row) */
    
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

    character_choice = get_player_choice(
        character_menu, character, previous_choice, CLASS_COL, 22, character_aux_hook, true);

    /* No selection? */
    if (character_choice == INVALID_CHOICE)
    {
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

/*
 * Helper function for 'player_birth()'.
 *
 * This function allows the player to select a race and character template, and
 * modify options (including the birth options).
 */
NavResult character_creation(void)
{
    int i, j;

    int phase = 1;

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
        birth_prompt_label('b', "b", back_label, sizeof(back_label));
        birth_prompt_label('o', "o", options_label, sizeof(options_label));
        birth_prompt_label('s', "s", scores_label, sizeof(scores_label));
        birth_prompt_label('f', "f", full_desc_label, sizeof(full_desc_label));
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
                return NAV_TO_MAIN; /* Esc at first screen → back to main menu */
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
                phase = 1;          /* Esc here → go back to race */
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
    // Bonus abilities
    /* grant *all* parsed character abilities */
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
        }
    }

    /* Set adult options from birth options */
    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
    {
        op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];
    }

    /* Reset score options from cheat options */
    for (i = OPT_CHEAT; i < OPT_ADULT; i++)
    {
        op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];
    }

    /* Set defaults for run-specific settings unless this is an old game file.
     * App-wide presentation/settings values are loaded from sil_sdl.json. */
    if (strlen(op_ptr->full_name) == 0)
    {
        op_ptr->vault_drop_frequency = VDF_NORMAL;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    }

    /* Ensure main_combat_rolls has a valid value for existing saves */
    if (op_ptr->main_combat_rolls > 4)
    {
        op_ptr->main_combat_rolls = 0;  /* Default to 0 lines */
    }

    /* Ensure ability_desc_mode has a valid value for existing saves */
    if (op_ptr->ability_desc_mode > 2)
    {
        op_ptr->ability_desc_mode = 0;
    }

    /* Ensure vault_drop_frequency has a valid value for existing saves */
    if (op_ptr->vault_drop_frequency > VDF_PLENTIFUL)
    {
        op_ptr->vault_drop_frequency = VDF_NORMAL;
    }

    if (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_OFF)
    {
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
    }

    if (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_OFF)
    {
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;
    }

    if (op_ptr->intro_style > INTRO_STYLE_RANDOM)
    {
        op_ptr->intro_style = INTRO_STYLE_RANDOM;
    }

    if (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
    {
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    }

    /* reset squelch bits */

    for (i = 0; i < z_info->k_max; i++)
    {
        k_info[i].squelch = SQUELCH_NEVER;
    }
    /*Clear the squelch bytes*/
    for (i = 0; i < SQUELCH_BYTES; i++)
    {
        squelch_level[i] = SQUELCH_NONE;
    }
    /* Clear the special item squelching flags */
    for (i = 0; i < z_info->e_max; i++)
    {
        e_info[i].aware = false;
        e_info[i].squelch = false;
    }

    /* Clear */
    Term_clear();

    log_debug("Character creation step completed: %s %s", p_name + p_info[p_ptr->prace].name, c_name + c_info[p_ptr->pcharacter].name);

    /* Done */
    return NAV_OK;

}

/*
 * Helper function to display wrapped text at a given position
 * Returns the number of lines used
 */
static int display_wrapped_text(cptr text, int start_col, int start_row, int max_width, byte color)
{
    if (!text || !text[0]) return 0;
    
    /* Get actual terminal size if max_width is not specified */
    int term_width, term_height;
    Term_get_size(&term_width, &term_height);
    
    if (max_width <= 0) {
        max_width = term_width - start_col - 2; /* Leave some margin */
    }
    
    char line_buffer[256]; /* Increased buffer size for wider terminals */
    int row = start_row;
    int line_pos = 0;
    const char* text_ptr = text;
    
    while (*text_ptr && row < term_height - 1) { /* Use actual terminal height */
        /* Skip leading spaces at start of line */
        while (*text_ptr == ' ' && line_pos == 0) text_ptr++;
        
        if (*text_ptr == '\n') {
            /* Explicit line break */
            line_buffer[line_pos] = '\0';
            if (line_pos > 0) {
                Term_putstr(start_col, row, -1, color, line_buffer);
                row++;
            }
            line_pos = 0;
            text_ptr++;
            continue;
        }
        
        if (line_pos >= max_width) {
            /* Need to wrap - find last space for word boundary */
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buffer[wrap_pos] != ' ') {
                wrap_pos--;
            }
            
            if (wrap_pos > 0) {
                /* Found a space - wrap at word boundary */
                line_buffer[wrap_pos] = '\0';
                Term_putstr(start_col, row, -1, color, line_buffer);
                
                /* Move remaining text to next line */
                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++) {
                    line_buffer[i] = line_buffer[wrap_pos + 1 + i];
                }
                line_pos = remaining;
            } else {
                /* No space found - hard wrap */
                line_buffer[line_pos] = '\0';
                Term_putstr(start_col, row, -1, color, line_buffer);
                line_pos = 0;
            }
            row++;
            continue;
        }
        
        /* Add character to current line */
        line_buffer[line_pos++] = *text_ptr++;
    }
    
    /* Display final line if any */
    if (line_pos > 0) {
        line_buffer[line_pos] = '\0';
        Term_putstr(start_col, row, -1, color, line_buffer);
        row++;
    }
    
    return row - start_row;
}

/*
 * Oath selection screen with three-column layout following abilities_menu2 pattern
 */
static NavResult select_oath(void)
{
    int available_mask = get_available_oaths_mask();
    
    /* If no oaths are available, skip oath selection */
    if (available_mask == 0) {
        p_ptr->oath_type = 0; /* No oath */
        log_debug("No oaths available, skipping oath selection");
        return NAV_OK;
    }
    
    int highlight = 1; /* Start highlighting first available oath */
    int choice = 0;
    int visible_count = 0;
    
    /* Find first available oath to highlight */
    for (int i = 1; z_info && i < z_info->oath_max; i++) {
        if (available_mask & (1 << (i - 1))) {
            highlight = i;
            break;
        }
    }
    
    while (true) {
        char buf[80];
        byte attr;
        
        /* Clear screen and use full-width layout */
        Term_clear();
        
        /* Title at the top center */
        Term_putstr(30, 0, -1, TERM_L_BLUE, "Choose your Oath");
        
        /* Setup oath list area (left side) */
        Term_putstr(2, 2, -1, TERM_WHITE, "Available Oaths");
        
        /* Build visible oaths list - only show available or broken */
        visible_count = 0;
        
        /* Always include "None" option */
        attr = (highlight == 0) ? TERM_L_BLUE : TERM_WHITE;
        Term_putstr(2, 4 + visible_count, -1, attr, format("%c) %s", 'a' + visible_count, oath_name_str(0)));
        visible_count++;
        
        /* Add available or broken oaths */
        for (int i = 1; z_info && i < z_info->oath_max; i++) {
            /* Skip locked oaths (not available and not broken) */
            if (!(available_mask & (1 << (i - 1))) && !oath_banned(i)) {
                continue;
            }
            
            /* Determine display color based on oath status and highlight */
            if (oath_banned(i)) {
                /* Broken oaths: red when not highlighted, bright red when highlighted */
                attr = (highlight == i) ? TERM_L_RED : TERM_RED;
                strnfmt(buf, 80, "%c) %s", 'a' + visible_count, oath_name_str(i));
            } else {
                /* Available oaths: bright blue when highlighted, white when not */
                attr = (highlight == i) ? TERM_L_BLUE : TERM_WHITE;
                strnfmt(buf, 80, "%c) %s", 'a' + visible_count, oath_name_str(i));
            }
            
            Term_putstr(2, 4 + visible_count, -1, attr, buf);
            visible_count++;
        }
        
        /* Display detailed description for highlighted oath in description column */
        wipe_screen_from(COL_DESCRIPTION);
        Term_putstr(COL_DESCRIPTION, 2, -1, TERM_WHITE, "Oath Details");
        
        if (highlight >= 0 && highlight < (z_info ? z_info->oath_max : 6)) {
            if (oath_banned(highlight) && highlight > 0) {
                /* Use oath-specific banned text */
                char* banned_text = oath_banned_text(highlight);
                if (banned_text && banned_text[0]) {
                    /* Display the oath-specific banned text with improved wrapping */
                    display_wrapped_text(banned_text, COL_DESCRIPTION, 4, 0, TERM_L_RED);
                } else {
                    /* Fallback to generic broken oath text */
                    Term_putstr(COL_DESCRIPTION, 4, -1, TERM_L_RED, "OATH BROKEN");
                    Term_putstr(COL_DESCRIPTION, 6, -1, TERM_RED, "\"Thy oath lies shattered,");
                    Term_putstr(COL_DESCRIPTION, 7, -1, TERM_RED, " thy word worthless as dust.\"");
                    Term_putstr(COL_DESCRIPTION, 9, -1, TERM_L_RED, "\"No Valar shall hear thy voice,");
                    Term_putstr(COL_DESCRIPTION, 10, -1, TERM_L_RED, " no light shall guide thy path.\"");
                    Term_putstr(COL_DESCRIPTION, 12, -1, TERM_RED, "Forever marked as oathbreaker");
                    Term_putstr(COL_DESCRIPTION, 13, -1, TERM_RED, "in this age.");
                }
            } else {
                /* Display oath description */
                if (highlight == 0) {
                    Term_putstr(COL_DESCRIPTION, 4, -1, TERM_SLATE, "Walk free of binding words");
                    Term_putstr(COL_DESCRIPTION, 6, -1, TERM_SLATE, "Take no oath and remain unbound");
                    Term_putstr(COL_DESCRIPTION, 7, -1, TERM_SLATE, "by sacred vows.");
                } else {
                    /* Get oath description and display it */
                    char* description = oath_description(highlight);
                    if (description && description[0]) {
                        Term_putstr(COL_DESCRIPTION, 4, -1, TERM_YELLOW, "Description:");
                        
                        /* Display description with improved word wrapping */
                        int row = 5;
                        row += display_wrapped_text(description, COL_DESCRIPTION, row, 0, TERM_SLATE);
                        
                        /* Display Pledge (P:) */
                        char* pledge = oath_pledge(highlight);
                        if (pledge && pledge[0]) {
                            char pledge_full[512];
                            strnfmt(pledge_full, sizeof(pledge_full), "Pledge: %s", pledge);
                            row += display_wrapped_text(pledge_full, COL_DESCRIPTION, row, 0, TERM_L_BLUE);
                        }
                        
                        /* Display Reward (R:) - MOVED TO TOP FOR VISIBILITY */
                        char* reward = oath_reward_text(highlight);
                        log_debug("Oath %d reward text: '%s'", highlight, reward ? reward : "NULL");
                        if (reward && reward[0]) {
                            char reward_full[512];
                            strnfmt(reward_full, sizeof(reward_full), "Reward: %s", reward);
                            row += display_wrapped_text(reward_full, COL_DESCRIPTION, row, 0, TERM_L_GREEN);
                        }
                        
                        /* Display Forbidden (F:) */
                        char* forbidden = oath_forbidden(highlight);
                        if (forbidden && forbidden[0]) {
                            char forbidden_full[512];
                            strnfmt(forbidden_full, sizeof(forbidden_full), "Forbidden: %s", forbidden);
                            row += display_wrapped_text(forbidden_full, COL_DESCRIPTION, row, 0, TERM_L_RED);
                        }
                    }
                }
            }
        }
        
        /* Footer text at bottom */
        Term_putstr(2, 20, -1, TERM_SLATE, "Oaths are sacred vows that grant power but bind your actions.");
        Term_putstr(2, 21, -1, TERM_SLATE, "Breaking an oath brings curse and shame.");
        
        /* Instructions at bottom with arrows */
        if (steamdeck_controls_active()) {
            char confirm_label[16];
            char back_label[16];
            char prompt_buf[160];

            birth_prompt_label(' ', "A", confirm_label, sizeof(confirm_label));
            birth_prompt_label('b', "b", back_label, sizeof(back_label));

            strnfmt(prompt_buf, sizeof(prompt_buf),
                "D-pad to Navigate     %s Accept     %s Back",
                confirm_label, back_label);
            Term_putstr(2, 23, -1, TERM_SLATE, prompt_buf);
        } else {
            Term_putstr(2, 23, -1, TERM_SLATE,
                "Arrows to Navigate     Enter/Space Accept     Esc Back");
        }
        
        /* Get input */
        char key = inkey();
        
        /* Handle input */
        if (steamdeck_controls_active() && key == 'b')
            return NAV_BACK; /* Go back to character creation */
        if (key == ESCAPE || key == 'q') {
            return NAV_BACK; /* Go back to character creation */
        }
        
        if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
            /* Select current highlighted option */
            if (highlight == 0 || ((available_mask & (1 << (highlight - 1))) && !oath_banned(highlight))) {
                choice = highlight;
                break;
            }
        }
        
        if (key >= 'a' && key <= 'a' + (z_info ? z_info->oath_max : 6) - 1) {
            /* Map letter selection to actual oath index */
            int display_pos = key - 'a';
            int actual_index = 0;
            int current_pos = 0;
            
            /* Find the actual oath index for this display position */
            for (int i = 0; z_info && i < z_info->oath_max; i++) {
                /* Skip locked oaths */
                if (i > 0 && !(available_mask & (1 << (i - 1))) && !oath_banned(i)) {
                    continue;
                }
                
                if (current_pos == display_pos) {
                    actual_index = i;
                    break;
                }
                current_pos++;
            }
            
            /* Select if valid */
            if (actual_index == 0 || ((available_mask & (1 << (actual_index - 1))) && !oath_banned(actual_index))) {
                choice = actual_index;
                break;
            }
        }
        
        /* Arrow key navigation: Up and Down */
        if (key == '8') {
            /* Move highlight to previous available or broken oath */
            int direction = -1;
            int new_highlight = highlight;
            
            do {
                new_highlight += direction;
                if (new_highlight < 0) new_highlight = z_info ? z_info->oath_max - 1 : 5;
                if (new_highlight >= (z_info ? z_info->oath_max : 6)) new_highlight = 0;
                
                /* Check if this option should be displayed */
                if (new_highlight == 0 || 
                    (available_mask & (1 << (new_highlight - 1))) || 
                    oath_banned(new_highlight)) {
                    highlight = new_highlight;
                    break;
                }
            } while (new_highlight != highlight);
        }
        
        if (key == '2') {
            /* Move highlight to next available or broken oath */
            int direction = 1;
            int new_highlight = highlight;
            
            do {
                new_highlight += direction;
                if (new_highlight < 0) new_highlight = z_info ? z_info->oath_max - 1 : 5;
                if (new_highlight >= (z_info ? z_info->oath_max : 6)) new_highlight = 0;
                
                /* Check if this option should be displayed */
                if (new_highlight == 0 || 
                    (available_mask & (1 << (new_highlight - 1))) || 
                    oath_banned(new_highlight)) {
                    highlight = new_highlight;
                    break;
                }
            } while (new_highlight != highlight);
        }
    }
    
    /* Set the chosen oath */
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

/* Forward declaration: used by compact skill allocation rendering. */
static int skill_cost(int base, int points);

static void birth_display_name_centered(void)
{
    char name[80];
    int wid = 80;
    int hgt = 24;
    int col = 0;

    Term_get_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    (void)hgt;

    if (p_ptr->oaths_broken)
        strnfmt(name, sizeof(name), "%s the Oathbreaker", op_ptr->full_name);
    else
        strnfmt(name, sizeof(name), "%s%s", op_ptr->full_name,
            c_name + current_character_profile->alt_name);

    int name_len = (int)strlen(name);
    if (name_len < wid)
        col = (wid - name_len) / 2;
    if (col < 0)
        col = 0;

    Term_erase(0, 0, 255);
    c_put_str(p_ptr->oaths_broken ? TERM_RED : TERM_L_BLUE, name, 0, col);
}

static void birth_display_stats_allocation_compact(const int stats[A_MAX], int selected,
    int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    char buf[160];
    char stat_buf[16];
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
        int cost = birth_stat_costs[stats[selected] + 4];
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
        char confirm_label[16];
        char back_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
        birth_prompt_label('q', "Start", quit_label, sizeof(quit_label));

        strnfmt(buf, sizeof(buf), "D-pad alloc  %s back  %s ok  %s quit",
            back_label, confirm_label, quit_label);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "8/2 select  4/6 adjust  ESC back  ENTER ok  q quit");
    }

    c_put_str(TERM_SLATE, buf, prompt_row, 1);
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
            strnfmt(buf, sizeof(buf), "ESC back to assignment  ENTER continue");
        }

        c_put_str(TERM_SLATE, buf, prompt_row, 1);

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;

        if ((ch == ESCAPE) || (ch == '4') || (ch == 'q') || (ch == 'Q'))
            return false;

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
            || (steamdeck && ch == steamdeck_confirm_key()))
            return true;
    }
}

static void birth_display_skill_allocation_compact(int selected_skill, const int old_base[S_MAX],
    const int skill_gain[S_MAX], int points_left, bool steamdeck)
{
    int wid = 80;
    int hgt = 24;
    char buf[160];
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
        char confirm_label[16];
        char back_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        strnfmt(buf, sizeof(buf), "D-pad alloc  %s back  %s ok  %s quit",
            back_label, confirm_label, quit_label);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "8/2 select  4/6 adjust  ESC back  ENTER ok  q quit");
    }

    c_put_str(TERM_SLATE, buf, prompt_row, 1);
}

/*
 * Helper function for 'player_birth()'.
 */
static NavResult player_birth_aux_2(void)
{
    int i;

    int row = 1;
    int col = 42;

    int stat = 0;

    int stats[A_MAX];

    int cost;

    char ch;

    char buf[80];

    /* Initialize stats */
    for (i = 0; i < A_MAX; i++)
    {
        /* Initial stats */
        stats[i] = p_ptr->stat_base[i];
    }

    /* Determine experience and things */
    get_extra();

    /* Show tutorial for first-time players (when scorefile is empty) */
    /* Do this AFTER get_extra() so character has stats/abilities to display */
    /* Check every time - show tutorial for every new character if scores file is empty */
    log_debug("Checking if tutorial should be shown...");
    bool is_empty = highscore_is_empty();
    log_debug("highscore_is_empty() returned: %s", is_empty ? "true" : "false");
    
    if (is_empty)
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
            birth_display_stats_allocation_compact(stats, stat, MAX_COST - cost, steamdeck);
        }
        else
        {
            int prompt_row = birth_prompt_row();

            /* Display the player */
            display_player(0);

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
                    strnfmt(buf, sizeof(buf), "%4d", birth_stat_costs[stats[i] + 4]);
                    c_put_str(attr, buf, row + i, sheet_col + 32);
#else
                    strnfmt(buf, sizeof(buf), "%4d*", birth_stat_costs[stats[i] + 4]);
                    c_put_str(attr, buf, row + i, sheet_col + 32);
                    c_put_str(attr, "*", row + i, sheet_col - 2);
#endif
                }
                else
                {
                    byte attr = TERM_L_WHITE;
                    strnfmt(buf, sizeof(buf), "%4d", birth_stat_costs[stats[i] + 4]);
                    c_put_str(attr, buf, row + i, sheet_col + 32);
                }
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

                /* Steam Deck UI: A=confirm, B=back, Start=quit */
                birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
                birth_prompt_label('q', "Start", quit_label, sizeof(quit_label));

                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "D-pad allocate  %s back  %s confirm  %s quit",
                    back_label, confirm_label, quit_label);
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt_buf);
            } else {
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE,
                    "Arrows -allocate    ESC -back   ENTER -confirm   q -quit");
            }

            if (story_character_enabled()) {
                sdl_story_font_disable();
            }
        }

        /* Get key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        /* Quit -> return to main menu before the game starts */
        if ((ch == 'Q') || (ch == 'q')) {
            if (turn == 0) return NAV_TO_MAIN;
            return NAV_QUIT;
        }

        /* Back to Character Selection */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
            return NAV_BACK;

        /* Done */
        if ((ch == '\r') || (ch == '\n') || (steamdeck && ch == steamdeck_confirm_key()))
            return NAV_OK;

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
    return NAV_BACK;
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

/*
 * Increase your skills by spending experience points
 */
extern NavResult gain_skills(void)
{
    int i;

    int row = 6;
    int col = 42;

    int skill = 0;

    int old_base[S_MAX];
    int skill_gain[S_MAX];

    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;

    char ch;

    char buf[80];

    NavResult result = NAV_OK;

    int tab = 0;

    log_debug("Starting skills allocation with %d experience points", p_ptr->new_exp);

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
            birth_display_skill_allocation_compact(skill, old_base, skill_gain, p_ptr->new_exp, steamdeck);
        }
        else
        {
            int prompt_row = birth_prompt_row();

            /* Display the player */
            display_player(0);

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

                /* Steam Deck UI: A=confirm, B=back, Start=quit */
                birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label, sizeof(confirm_label));
                birth_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
                birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

                strnfmt(prompt_buf, sizeof(prompt_buf),
                    "D-pad -allocate      %s-back     %s-confirm     %s-quit",
                    back_label, confirm_label, quit_label);
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt_buf);
            } else {
                Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE,
                    "Arrows -allocate      ESC -back     ENTER -confirm     q -quit");
            }

            if (story_character_enabled()) {
                sdl_story_font_disable();
            }
        }

        /* Get key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        /* Quit -> back to main menu before the game starts */
        if (((ch == 'Q') || (ch == 'q')) && (turn == 0)) {
            /* restore state before leaving */
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            skill_gain_in_progress = false;
            return NAV_TO_MAIN;
        }

        /* Done */
        if ((ch == '\r') || (ch == '\n') || (steamdeck && ch == steamdeck_confirm_key()))
        {
            if (compact)
            {
                if (!birth_show_compact_description_after_assignment(steamdeck))
                    continue;
            }
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
            result = NAV_BACK;   /* go back to Character Selection */
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
    log_debug("Entering oath selection");
    NavResult oath_result = select_oath();
    if (oath_result != NAV_OK) return oath_result;
    log_debug("Oath selection completed");

    /* Point-based flow */
    for (;;)
    {
        display_player(0);

        /* Stats allocation screen */
        log_debug("Entering stats allocation");
        NavResult s = player_birth_aux_2();
        if (s == NAV_OK) {
            /* Skill allocation: may return NAV_BACK / NAV_TO_MAIN */
            log_debug("Stats accepted, entering skills allocation");
            NavResult g = gain_skills();
            if (g != NAV_OK) return g;
            log_debug("Skills allocation completed");
            break; /* accepted */
        }
        if (s == NAV_BACK)   return NAV_BACK;    /* back to Character Selection */
        if (s == NAV_TO_MAIN) return NAV_TO_MAIN;/* back to main menu */
        if (s == NAV_QUIT)   return NAV_QUIT;    /* hard exit */
        /* any other value: loop again */
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
    metarun_load_persistent_settings();

    /* Reapply app-wide settings after character creation so UI preferences are
     * not sourced from the metarun or savefile. */
    sdl_config_load_app_options(get_sdl_config_path());

    log_info("Character creation completed: %s the %s", op_ptr->full_name, p_name + rp_ptr->name);

    return NAV_OK;
}














