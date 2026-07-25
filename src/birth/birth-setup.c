/* File: birth/birth-setup.c */

#include "angband.h"
#include "birth/birth-internal.h"

static bool starting_artifact_is_eligible(int art_idx, int k_idx);

static void copy_start_items(start_item dest[MAX_START_ITEMS],
    const start_item src[MAX_START_ITEMS]);
static void replace_start_food(start_item list[MAX_START_ITEMS], byte from_sval,
    byte to_sval);

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

int get_start_xp(void)
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
int curses_stat_adj(int s)   /* s = 0-3  (STR-DEX-CON-GRA) */
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
void get_extra(void)
{
    int i, j;
    
    p_ptr->new_exp = p_ptr->exp = get_start_xp();
    p_ptr->discovery_lore_flags = 0;
    p_ptr->quick_access_prompt_flags = 0;
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
    log_debug("birth: character_generated set to false - starting character wipe");
    int i;
    char history[550];

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
    }

    /* Wipe the player */
    memset(p_ptr, 0, sizeof(player_type));

    turn = 0;
    playerturn = 0;
    min_depth_counter = 0;

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
    }

    for (i = 0; i < A_MAX; i++)
        p_ptr->stat_base[i] = 0;

    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->song_target_song = SNG_NOTHING;

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

    /* Aulë quest init */
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
    
    /* Nienna quest init */
    p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
    p_ptr->niena_level = 0;
    
    /* Oromë quest init */
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
    for (i = 0; i < 15; i++) p_ptr->quest_reserved[i] = 0; /* quest_reserved[0] = initiated quest count; quest_reserved[1..6] = per-run quest completion markers */

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
 *              "Crown of Fëanor" matches character "Fëanor".
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

void player_outfit(void)
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

void finalize_character_creation_selection(void)
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

    op_ptr->main_combat_rolls = 0;
    if (op_ptr->vault_drop_frequency > VDF_PLENTIFUL)
        op_ptr->vault_drop_frequency = VDF_NORMAL;
    if (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_OFF)
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
    if (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_BANNER_DELAY)
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER_DELAY;
    if (op_ptr->narrative_banner_turns > NARRATIVE_BANNER_TURNS_MAX)
        op_ptr->narrative_banner_turns = DEFAULT_NARRATIVE_BANNER_TURNS;
    if (op_ptr->intro_style > INTRO_STYLE_RANDOM)
        op_ptr->intro_style = INTRO_STYLE_RANDOM;
    if (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    if (op_ptr->min_depth_timer_mode > MIN_DEPTH_TIMER_MODE_MAX)
        op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;

    for (i = 0; i < z_info->e_max; i++)
        e_info[i].aware = false;

    Term_clear();

    log_debug("Character creation step completed: %s %s",
        p_name + p_info[p_ptr->prace].name,
        c_name + c_info[p_ptr->pcharacter].name);
}

/*
 * Initial stat costs.
 */
const int birth_stat_costs[11]
    = { -4, -3, -2, -1, 0, 1, 3, 6, 10, 15, 21 };

int birth_stat_increase_cost(int stat)
{
    int current_index = stat + 4;
    int next_index = current_index + 1;

    if (current_index < 0 || next_index < 0)
        return 0;
    if (next_index >= (int)N_ELEMENTS(birth_stat_costs))
        return 0;

    return birth_stat_costs[next_index] - birth_stat_costs[current_index];
}
