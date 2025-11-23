/* File: generate.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include <SDL3/SDL.h>
/* Ensure C library prototypes are visible for tools */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
/* Quest vault debug instrumentation */
#define DEBUG_QUEST_VAULT 0
#if DEBUG_QUEST_VAULT
static int qv_y1 = -1, qv_x1 = -1, qv_y2 = -1, qv_x2 = -1;
static int qv_h = 0, qv_w = 0;
static unsigned short *qv_feat_snapshot = NULL;

static void qv_capture(void) {
    int y,x;
    if (qv_y1 < 0) return;
    qv_h = qv_y2 - qv_y1 + 1;
    qv_w = qv_x2 - qv_x1 + 1;
    mem_free_null(qv_feat_snapshot);
    qv_feat_snapshot = mem_alloc_array(qv_h * qv_w, unsigned short);
    for (y = qv_y1; y <= qv_y2; ++y)
        for (x = qv_x1; x <= qv_x2; ++x)
            qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)] = cave_feat[y][x];
    log_trace("Quest vault DEBUG: snapshot captured (%d x %d) bounds (%d,%d)-(%d,%d)", qv_h, qv_w, qv_y1, qv_x1, qv_y2, qv_x2);
}

static char qv_glyph(int f) {
    switch (f) {
        case FEAT_FLOOR: return '.'; case FEAT_WALL_OUTER: return '#';
        case FEAT_WALL_INNER: return '+'; case FEAT_WALL_EXTRA: return 'X';
#ifdef FEAT_DOOR_CLOSED
        case FEAT_DOOR_CLOSED: return 'D';
#endif
        case FEAT_FORGE_HEAD: case FEAT_FORGE_TAIL: return 'F';
        default: return '?';
    }
}

static void qv_dump(const char *phase) {
    if (qv_y1 < 0) return;
    int y,x; char row[256];
    log_trace("Quest vault DEBUG: layout (%s) bounds (%d,%d)-(%d,%d)", phase, qv_y1, qv_x1, qv_y2, qv_x2);
    for (y = qv_y1; y <= qv_y2; ++y) {
        int idx=0;
        for (x = qv_x1; x <= qv_x2 && idx < (int)sizeof(row)-2; ++x)
            row[idx++] = qv_glyph(cave_feat[y][x]);
        row[idx]='\0';
        log_trace("Quest vault DEBUG ROW %2d: %s", y, row);
    }
}

static void qv_compare(void) {
    if (!qv_feat_snapshot) return;
    int diffs=0, y,x;
    for (y = qv_y1; y <= qv_y2; ++y) for (x = qv_x1; x <= qv_x2; ++x) {
        unsigned short before = qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)];
        unsigned short now = cave_feat[y][x];
        if (before != now) {
            log_trace("Quest vault DEBUG: tile changed (%d,%d) %d->%d", y, x, before, now);
            if (++diffs >= 50) goto done_diffs;
        }
    }
done_diffs:
    if (!diffs) log_trace("Quest vault DEBUG: no tile changes detected since snapshot");
}
#endif

typedef struct vault_monster_spec {
    char symbol;
    const char* guid_text;
    u64b guid;
    bool guid_cached;
    bool start_sleeping;
    bool ignore_depth;
} vault_monster_spec;

static vault_monster_spec vault_monster_table[] = {
    {'C', "9cbdbb88fd4f59dc", 0, false, true, true},
    {'H', "c790972955718680", 0, false, true, false},
    {'@', "4acd2c9fcc5cd6e5", 0, false, true, false},
    {'o', "88ef7547642967b2", 0, false, true, false},
    {'O', "2c739cdb1be99f2c", 0, false, true, false},
    {'Z', "05f49e29acf49a93", 0, false, true, true},
    {'f', "3c10b33361f6f136", 0, false, true, false},
    {'F', "9a6fbc6e7b46f502", 0, false, true, false},
    {'T', "b39a82dfdc1c5ef9", 0, false, true, false},
    {'W', "c92f7e02e189e1bd", 0, false, true, true},
    {'y', "2f6ec4ab45007365", 0, false, true, false},
    {'Y', "0af151dfe09fe455", 0, false, true, false},
    {'A', "ed37fc4fce32643f", 0, false, true, true},
    {'L', "d27e36edf5c2f432", 0, false, true, true},
    {'N', "f134bcd795c27d4f", 0, false, true, true},
    {'D', "3ab7e216cb871fec", 0, false, true, true},
    {'R', "0e0f11695f8a443d", 0, false, true, true},
    {'U', "c2485b83ba33934d", 0, false, true, true},
    {'G', "7b038638b2981d20", 0, false, true, true},
    {'V', "58d8cf770bfcbe6f", 0, false, true, true},
    {'B', "9c44dec3f9d6d14c", 0, false, false, true}, /* Duruin, Least of the Balrogs */
    {'q', "ccff426ff2ef0318", 0, false, true, true},  /* Whispering shadow */
    {'j', "d5e4892102e9b48a", 0, false, true, true},  /* Shadow spider */
    {'k', "d2d2f0b7edcf4cf6", 0, false, true, true},  /* Lurking horror */
    {'n', "7783062d13500802", 0, false, true, true},  /* Nightthorn */
};

static bool place_vault_monster_token(char symbol, int y, int x)
{
    for (size_t i = 0; i < N_ELEMENTS(vault_monster_table); i++)
    {
        vault_monster_spec* spec = &vault_monster_table[i];
        if (spec->symbol != symbol)
            continue;

        if (!spec->guid_cached)
        {
            spec->guid_cached = true;
            if (!parse_u64b_hex(spec->guid_text, &spec->guid))
            {
                spec->guid = 0;
                log_error("Vault: invalid GUID '%s' for token '%c'",
                    spec->guid_text, symbol);
            }
        }

        if (!spec->guid)
        {
            log_warn("Vault: GUID missing for monster token '%c'", symbol);
            return false;
        }

        if (!place_monster_by_guid(
                y, x, spec->guid, spec->start_sleeping, spec->ignore_depth, NULL))
        {
            log_warn("Vault: failed to place monster for token '%c'", symbol);
            return false;
        }

        return true;
    }

    return false;
}

/* Structure to hold pending quest state changes that should only be applied
 * when level generation is completely successful */
typedef struct {
    bool has_aule_change;
    bool has_mandos_change;
    bool has_varda_change;
    int aule_level;
    int mandos_level;
    int varda_level;
    int aule_forge_y, aule_forge_x;
    int mandos_vault_y, mandos_vault_x;
    int varda_vault_y, varda_vault_x;
} pending_quest_states_t;

/* Global variable to track pending quest state changes */
static pending_quest_states_t pending_quest_states = {0};

/* Quest lottery system: determines which quest (if any) "wins" this level */
static int quest_lottery_winner = 0; /* 0=none, quest_id=winner (1=Tulkas, 4=Niena, etc.) */
static bool quest_lottery_resolved = false; /* true once lottery is run for this level */

/* Function to run the quest lottery once per level - determines which quest (if any) gets this level */
/* Roulette quest registry entry */
typedef struct {
    int quest_id;           /* Quest index from quest.txt */
    byte* quest_state_ptr;  /* Pointer to quest state variable */
    u32b metarun_quest_id;  /* Metarun completion tracking ID */
    bool (*eligibility_check)(int depth, int quest_id); /* Custom eligibility function */
    bool (*probability_roll)(int depth, int quest_id);  /* Custom probability function */
} roulette_quest_entry;

/* Forward declarations for quest-specific functions */
static bool data_driven_eligibility_check(int depth, int quest_id);
static bool tulkas_probability_roll(int depth, int quest_id);
static bool niena_probability_roll(int depth, int quest_id);
static void run_quest_lottery(void);

/* Generic parametric formula-based functions */
static bool generic_eligibility_check(int depth, int quest_id);
static bool generic_probability_roll(int depth, int quest_id);

/* Parametric formula calculation */
static float calculate_parametric_probability(quest_type* q_ptr, int depth);

/* Determine if metarun history blocks a quest (unless oath override applies) */
static bool quest_metarun_blocked(int quest_id, u32b metarun_flag)
{
    if (!metarun_flag) return false;

    int completion_count = metarun_quest_completion_count(metarun_flag);
    quest_type* q_ptr = (quest_id > 0 && quest_id < z_info->quest_max) ? &quest_info[quest_id] : NULL;
    byte oath_id = q_ptr ? q_ptr->oath_id : 0;
    bool oath_override = false;

    if (oath_id > 0 && p_ptr && p_ptr->oath_type == oath_id && !oath_invalid(oath_id)) {
        oath_override = true;
    }

    if (completion_count >= METARUN_QUEST_COMPLETION_CAP) {
        log_trace("Quest %d blocked by metarun cap (%d/%d)", quest_id, completion_count, METARUN_QUEST_COMPLETION_CAP);
        return true;
    }

    if (completion_count > 0 && !oath_override) {
        log_trace("Quest %d blocked by prior completion (%d) without oath override", quest_id, completion_count);
        return true;
    }
    if (completion_count > 0 && oath_override) {
        log_trace("Quest %d eligible again: %d prior completion(s) but oath %d is active", quest_id, completion_count, oath_id);
    }

    return false;
}

/* Roulette quest registry - initialized dynamically based on Y:1 field */
static roulette_quest_entry roulette_quests[8];  /* Max 8 quests from limits.txt */
static int roulette_quest_count = 0;

/* Parametric formula calculation */
static float calculate_parametric_probability(quest_type* q_ptr, int depth) {
    float probability = 0.0f;
    
    /* Check depth bounds */
    if (depth < q_ptr->depth_min || depth > q_ptr->depth_max) {
        log_trace("Quest %d: depth %d outside valid range [%d-%d], probability = 0", 
                  q_ptr->quest_num, depth, q_ptr->depth_min, q_ptr->depth_max);
        return 0.0f;
    }
    
    log_debug("Quest %d formula calculation: type=%d, depth=%d, params=[%.3f,%.3f,%.3f,%.3f]", 
              q_ptr->quest_num, q_ptr->formula_type, depth, 
              q_ptr->formula_params[0], q_ptr->formula_params[1], 
              q_ptr->formula_params[2], q_ptr->formula_params[3]);
    
    switch (q_ptr->formula_type) {
        case FORMULA_LINEAR_DECAY:
            /* Formula: 1/(base - depth) */
            /* Params: [0]=base, others unused */
            {
                float base = q_ptr->formula_params[0];
                float denominator = base - (float)depth;
                if (denominator > 0.0f) {
                    probability = 1.0f / denominator;
                    log_debug("Quest %d LINEAR_DECAY: base=%.1f, depth=%d, denominator=%.1f, probability=%.4f", 
                              q_ptr->quest_num, base, depth, denominator, probability);
                } else {
                    log_debug("Quest %d LINEAR_DECAY: base=%.1f, depth=%d, denominator=%.1f <= 0, probability=0", 
                              q_ptr->quest_num, base, depth, denominator);
                }
            }
            break;
            
        case FORMULA_SCALED_RANGE:
            /* Formula: max_prob * max(0, min(1, (depth-start_depth)/range)) */
            /* Params: [0]=max_prob, [1]=start_depth, [2]=range, [3]=unused */
            {
                float max_prob = q_ptr->formula_params[0];
                float start_depth = q_ptr->formula_params[1];
                float range = q_ptr->formula_params[2];
                
                if (range > 0.0f) {
                    float factor = ((float)depth - start_depth) / range;
                    if (factor < 0.0f) factor = 0.0f;
                    if (factor > 1.0f) factor = 1.0f;
                    probability = max_prob * factor;
                    log_debug("Quest %d SCALED_RANGE: max_prob=%.3f, start_depth=%.1f, range=%.1f, depth=%d, factor=%.3f, probability=%.4f", 
                              q_ptr->quest_num, max_prob, start_depth, range, depth, factor, probability);
                } else {
                    log_debug("Quest %d SCALED_RANGE: range=%.1f <= 0, probability=0", q_ptr->quest_num, range);
                }
            }
            break;
            
        case FORMULA_FIXED_PERCENT:
            /* Formula: constant percentage */
            /* Params: [0]=percentage (0.0-1.0), others unused */
            probability = q_ptr->formula_params[0];
            log_debug("Quest %d FIXED_PERCENT: constant probability=%.4f", q_ptr->quest_num, probability);
            break;
            
        case FORMULA_LINEAR_INTERPOLATE:
            /* Formula: linear interpolation between min_prob and max_prob over depth range */
            /* Params: [0]=min_prob, [1]=max_prob, [2]=unused, [3]=unused */
            {
                float min_prob = q_ptr->formula_params[0];
                float max_prob = q_ptr->formula_params[1];
                int depth_range = q_ptr->depth_max - q_ptr->depth_min;
                
                if (depth_range > 0) {
                    float factor = (float)(depth - q_ptr->depth_min) / (float)depth_range;
                    probability = min_prob + (max_prob - min_prob) * factor;
                    log_debug("Quest %d LINEAR_INTERPOLATE: min_prob=%.3f, max_prob=%.3f, depth=%d, range=%d, factor=%.3f, probability=%.4f", 
                              q_ptr->quest_num, min_prob, max_prob, depth, depth_range, factor, probability);
                } else {
                    /* Single depth case - use min_prob */
                    probability = min_prob;
                    log_debug("Quest %d LINEAR_INTERPOLATE: depth_range=0, using min_prob=%.3f", q_ptr->quest_num, min_prob);
                }
            }
            break;
            
        case FORMULA_HARDCODED:
        default:
            /* Use hardcoded functions - should not reach here for parametric calls */
            probability = 0.0f;
            log_debug("Quest %d: HARDCODED or unknown formula type %d, probability=0", 
                      q_ptr->quest_num, q_ptr->formula_type);
            break;
    }
    
    /* Clamp probability to valid range */
    if (probability < 0.0f) probability = 0.0f;
    if (probability > 1.0f) probability = 1.0f;
    
    log_info("Quest %d final probability at depth %d: %.4f (%.2f%%)", 
             q_ptr->quest_num, depth, probability, probability * 100.0f);
    
    return probability;
}

/* Generic eligibility check for parametric quests */
static bool generic_eligibility_check(int depth, int quest_id) {
    /* Use the comprehensive eligibility check that handles E: field data */
    return check_quest_eligibility(quest_id, depth);
}

/* Generic probability roll for parametric quests */
static bool generic_probability_roll(int depth, int quest_id) {
    quest_type* q_ptr = &quest_info[quest_id];
    float probability = calculate_parametric_probability(q_ptr, depth);
    
    if (probability <= 0.0f) {
        log_trace("Quest lottery: Quest %d probability is 0%% at depth %d", quest_id, depth);
        return false;
    }
    
    /* Roll using a 0-9999 scale to preserve fractional probabilities */
    int threshold = (int)(probability * 10000.0f + 0.5f);
    if (threshold < 1) threshold = 1;
    if (threshold > 10000) threshold = 10000;
    int dice_roll = rand_int(10000);
    bool won = (dice_roll < threshold);
    
    if (won) {
        log_trace("Quest lottery: Quest %d WINS! (rolled %d < %d, chance was %.2f%%, formula_type=%d)", 
                 quest_id, dice_roll, threshold, probability * 100.0f, q_ptr->formula_type);
    } else {
        log_trace("Quest lottery: Quest %d roll failed (rolled %d >= %d, chance was %.2f%%, formula_type=%d)", 
                 quest_id, dice_roll, threshold, probability * 100.0f, q_ptr->formula_type);
    }
    
    return won;
}

/* Helper function to map quest state variable name to pointer */
static byte* get_quest_state_ptr(u32b var_name_offset) {
    if (!var_name_offset || !quest_name_text) return NULL;
    
    /* Get the actual variable name string */
    cptr actual_name = quest_name_text + var_name_offset;
    
    if (SDL_strcasecmp(actual_name, "tulkas_quest") == 0) {
        return &p_ptr->tulkas_quest;
    } else if (SDL_strcasecmp(actual_name, "aule_quest") == 0) {
        return &p_ptr->aule_quest;
    } else if (SDL_strcasecmp(actual_name, "mandos_quest") == 0) {
        return &p_ptr->mandos_quest;
    } else if (SDL_strcasecmp(actual_name, "niena_quest") == 0) {
        return &p_ptr->niena_quest;
    } else if (SDL_strcasecmp(actual_name, "orome_quest") == 0) {
        return &p_ptr->orome_quest;
    } else if (SDL_strcasecmp(actual_name, "varda_quest") == 0) {
        return &p_ptr->varda_quest;
    }
    
    return NULL; /* Unknown quest state variable */
}

/* Helper function to map metarun quest ID string to constant */
static int get_metarun_quest_id(u32b id_name_offset) {
    if (!id_name_offset || !quest_name_text) return 0;
    
    /* Get the actual ID string */
    cptr actual_id = quest_name_text + id_name_offset;
    
    if (SDL_strcasecmp(actual_id, "METARUN_QUEST_TULKAS") == 0) {
        return METARUN_QUEST_TULKAS;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_AULE") == 0) {
        return METARUN_QUEST_AULE;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_MANDOS") == 0) {
        return METARUN_QUEST_MANDOS;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_NIENA") == 0) {
        return METARUN_QUEST_NIENA;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_OROME") == 0) {
        return METARUN_QUEST_OROME;
    } else if (SDL_strcasecmp(actual_id, "METARUN_QUEST_VARDA") == 0) {
        return METARUN_QUEST_VARDA;
    }
    
    return 0; /* Unknown metarun quest ID */
}

/* Initialize the roulette quest registry based on quest.txt Y: field */
static void init_roulette_quest_registry(void) {
    roulette_quest_count = 0;
    
    /* Scan all quests to find Y:1 (roulette-based) quests */
    for (int i = 1; i < z_info->quest_max; i++) {
        quest_type* q_ptr = &quest_info[i];
        
        /* Skip if not a roulette quest (Y:1) */
        if (q_ptr->quest_type != 1) continue;
        
        /* Add to registry */
        roulette_quest_entry* entry = &roulette_quests[roulette_quest_count];
        entry->quest_id = i;
        
        /* Use data-driven mapping for quest state and metarun ID */
        entry->quest_state_ptr = get_quest_state_ptr(q_ptr->quest_state_var);
        entry->metarun_quest_id = get_metarun_quest_id(q_ptr->metarun_quest_id);
        
        /* Log mapping results */
        if (entry->quest_state_ptr && entry->metarun_quest_id) {
            log_trace("Quest lottery: Quest %d mapped successfully (state_ptr=%p, metarun_id=%d)", 
                      i, entry->quest_state_ptr, entry->metarun_quest_id);
        } else {
            log_trace("Quest lottery: Quest %d mapping failed (state_ptr=%p, metarun_id=%d)", 
                      i, entry->quest_state_ptr, entry->metarun_quest_id);
            /* Still register the quest but mark as unsupported for state tracking */
        }
        
        /* Use parametric formulas if available, otherwise skip unsupported quests */
        if (q_ptr->formula_type != FORMULA_HARDCODED) {
            entry->eligibility_check = generic_eligibility_check;
            entry->probability_roll = generic_probability_roll;
            log_trace("Quest lottery: Quest %d using parametric formula (type=%d)", i, q_ptr->formula_type);
        } else {
            /* For legacy compatibility, map hardcoded functions for known quests */
            if (i == 1) { /* Tulkas */
                entry->eligibility_check = data_driven_eligibility_check; /* Use data-driven eligibility */
                entry->probability_roll = tulkas_probability_roll;
                log_trace("Quest lottery: Quest %d using mixed formula (data-driven eligibility + hardcoded probability)", i);
            } else if (i == 4) { /* Niena */
                entry->eligibility_check = data_driven_eligibility_check; /* Use data-driven eligibility */
                entry->probability_roll = niena_probability_roll;
                log_trace("Quest lottery: Quest %d using mixed formula (data-driven eligibility + hardcoded probability)", i);
            } else {
                entry->eligibility_check = NULL;
                entry->probability_roll = NULL;
                log_trace("Quest lottery: Quest %d has no formula implementation, skipping", i);
                continue; /* Skip unsupported quests */
            }
        }
        
        roulette_quest_count++;
        log_trace("Quest lottery: Registered roulette quest %d (index %d)", 
                  entry->quest_id, roulette_quest_count - 1);
    }
    
    log_trace("Quest lottery: Initialized with %d roulette quests (data-driven mapping)", roulette_quest_count);
}

/* Data-driven eligibility check using quest.txt E: field */
static bool data_driven_eligibility_check(int depth, int quest_id) {
    return check_quest_eligibility(quest_id, depth);
}

/* Tulkas-specific probability roll */
static bool tulkas_probability_roll(int depth, int quest_id) {
    int tulkas_chance = 27 - depth;
    int dice_roll = rand_int(tulkas_chance);  /* Get the actual roll value */
    bool won = (dice_roll == 0);  /* one_in_(N) succeeds when rand_int(N) == 0 */
    
    if (won) {
        log_trace("Quest lottery: Tulkas WINS! (rolled %d, needed 0, chance was 1/%d = %.1f%%)", 
                 dice_roll, tulkas_chance, 100.0f / tulkas_chance);
    } else {
        log_trace("Quest lottery: Tulkas roll failed (rolled %d, needed 0, chance was 1/%d = %.1f%%)", 
                 dice_roll, tulkas_chance, 100.0f / tulkas_chance);
    }
    
    return won;
}

/* Niena-specific probability roll */
static bool niena_probability_roll(int depth, int quest_id) {
    /* Niena probability: p_Nienna(lvl) = 0.125 * max(0, min(1, (lvl - 14) / 5)) */
    /* This gives: 0% before lvl 14, scales from 0% to 12.5% between lvl 14-19, then 12.5% at 19+ */
    
    float depth_factor = (float)(depth - 14) / 5.0f;
    if (depth_factor > 1.0f) depth_factor = 1.0f;  /* cap at 1.0 */
    if (depth_factor < 0.0f) depth_factor = 0.0f;  /* shouldn't happen due to depth check above */
    
    float niena_probability = 0.125f * depth_factor;
    
    if (niena_probability > 0.0f) {
        /* Convert probability to one_in_() parameter: if P = 1/N, then one_in_(N) gives probability P */
        int niena_chance = (int)(1.0f / niena_probability + 0.5f);  /* round to nearest int */
        
        int dice_roll = rand_int(niena_chance);  /* Get the actual roll value */
        bool won = (dice_roll == 0);  /* one_in_(N) succeeds when rand_int(N) == 0 */
        
        if (won) {
            log_trace("Quest lottery: Niena WINS! (rolled %d, needed 0, chance was 1/%d = %.1f%%)", 
                     dice_roll, niena_chance, niena_probability * 100.0f);
        } else {
            log_trace("Quest lottery: Niena roll failed (rolled %d, needed 0, chance was 1/%d = %.1f%%)", 
                     dice_roll, niena_chance, niena_probability * 100.0f);
        }
        
        return won;
    } else {
        log_trace("Quest lottery: Niena probability is 0%% at depth %d", depth);
        return false;
    }
}

/* Debug function: manually trigger quest roulette */
void debug_run_quest_roulette(void) {
    /* Reset lottery state to allow a new run */
    quest_lottery_winner = 0;
    quest_lottery_resolved = false;
    
    run_quest_lottery();
}

/* Debug function: get quest lottery winner */
int debug_get_quest_lottery_winner(void) {
    return quest_lottery_winner;
}

static void run_quest_lottery(void) {
    log_trace("Quest lottery: === LOTTERY START === (depth=%d, quest_reserved[0]=%d)", p_ptr->depth, p_ptr->quest_reserved[0]);
    
    if (quest_lottery_resolved) {
        log_trace("Quest lottery: Already resolved for this level (winner=%d)", quest_lottery_winner);
        return;
    }
    
    /* If Varda quest is active/successful, suppress other roulette quests entirely */
    if (p_ptr->varda_quest >= VARDA_QUEST_ACTIVE) {
        log_trace("Quest lottery: SKIPPED - Varda quest in progress (state=%d)", p_ptr->varda_quest);
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        return;
    }
    
    /* Initialize registry if not done yet */
    if (roulette_quest_count == 0) {
        init_roulette_quest_registry();
    }
    
    /* CRITICAL: Do not run lottery if player is actively escaping (on the run) */
    if (p_ptr->on_the_run) {
        log_trace("Quest lottery: SKIPPED - player is on the run (no quests spawn during escape)");
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        return;
    }
    
    /* CRITICAL: Do not run lottery if any quest is already started on this character */
    log_trace("Quest lottery: Checking current quest states before lottery");
    log_trace("Quest lottery: tulkas=%d, niena=%d, orome=%d, aule=%d, mandos=%d, varda=%d", 
              p_ptr->tulkas_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->varda_quest);
    log_trace("Quest lottery: quest_reserved[0]=%d (any quest spawned flag - should block all quests if set)", p_ptr->quest_reserved[0]);
    
    /* Check if any quest slot is already reserved */
    if (p_ptr->quest_reserved[0]) {
        log_trace("Quest lottery: BLOCKED - quest slot already reserved (quest_reserved[0]=1), one-quest-per-run enforced");
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        return;
    }
    
    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED || 
        p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED ||
        p_ptr->orome_quest > OROME_QUEST_NOT_STARTED ||
        p_ptr->aule_quest > AULE_QUEST_NOT_STARTED ||
        p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED ||
        p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED) {
        
        log_trace("Quest lottery: SKIPPED - quest already started on this character (tulkas=%d, niena=%d, orome=%d, aule=%d, mandos=%d, varda=%d)", 
                  p_ptr->tulkas_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->varda_quest);
        quest_lottery_winner = 0;
        quest_lottery_resolved = true;
        return;
    }
    
    log_trace("Quest lottery: Running for depth %d with %d registered roulette quests", 
              p_ptr->depth, roulette_quest_count);
    
    /* Reset state */
    quest_lottery_winner = 0;
    quest_lottery_resolved = true;
    
    /* Create a randomized order for quest evaluation */
    int quest_order[8];  /* Max 8 quests */
    for (int i = 0; i < roulette_quest_count; i++) {
        quest_order[i] = i;
    }
    
    /* Shuffle the quest order using Fisher-Yates algorithm */
    for (int i = roulette_quest_count - 1; i > 0; i--) {
        int j = rand_int(i + 1);
        int temp = quest_order[i];
        quest_order[i] = quest_order[j];
        quest_order[j] = temp;
    }
    
    /* Log the randomized quest evaluation order */
    log_trace("Quest lottery: Random evaluation order generated for %d quests", roulette_quest_count);
    for (int i = 0; i < roulette_quest_count; i++) {
        roulette_quest_entry* entry = &roulette_quests[quest_order[i]];
        const char* quest_name = "Unknown";
        if (entry->quest_id > 0 && entry->quest_id < z_info->quest_max) {
            quest_type* q_ptr = &quest_info[entry->quest_id];
            if (q_ptr->name && quest_name_text) {
                quest_name = quest_name_text + q_ptr->name;
            }
        }
        log_trace("Quest lottery: Order position %d -> Quest %d (%s)", 
                  i, entry->quest_id, quest_name);
    }
    
    /* Evaluate quests in random order */
    for (int order_idx = 0; order_idx < roulette_quest_count; order_idx++) {
        int quest_idx = quest_order[order_idx];
        roulette_quest_entry* entry = &roulette_quests[quest_idx];
        
        /* Skip unsupported quests */
        if (!entry->quest_state_ptr || !entry->eligibility_check || !entry->probability_roll) {
            log_trace("Quest lottery: Skipping unsupported quest %d", entry->quest_id);
            continue;
        }
        
        /* Check quest state - must be NOT_STARTED */
        if (*entry->quest_state_ptr != 0) { /* 0 = NOT_STARTED for all quest types */
            log_trace("Quest lottery: Quest %d not eligible (state=%d)", 
                      entry->quest_id, *entry->quest_state_ptr);
            continue;
        }
        
        /* Check metarun history (respect oath overrides and completion cap) */
        if (quest_metarun_blocked(entry->quest_id, entry->metarun_quest_id)) {
            log_trace("Quest lottery: Quest %d not eligible due to metarun history", entry->quest_id);
            continue;
        }
        
        /* Check quest-specific eligibility */
        log_trace("Quest lottery: Checking eligibility for Quest %d at depth %d", entry->quest_id, p_ptr->depth);
        bool eligible = entry->eligibility_check(p_ptr->depth, entry->quest_id);
        log_trace("Quest lottery: Quest %d eligibility result: %s", entry->quest_id, eligible ? "PASS" : "FAIL");
        if (!eligible) {
            log_trace("Quest lottery: Quest %d failed eligibility check", entry->quest_id);
            continue;
        }
        
        /* Roll for quest probability */
        log_trace("Quest lottery: Evaluating Quest %d for probability roll at depth %d", entry->quest_id, p_ptr->depth);
        bool won_probability = entry->probability_roll(p_ptr->depth, entry->quest_id);
        log_trace("Quest lottery: Quest %d probability result: %s", entry->quest_id, won_probability ? "WON" : "LOST");
        if (won_probability) {
            quest_lottery_winner = entry->quest_id;
            log_trace("Quest lottery: Quest %d WINS the lottery!", entry->quest_id);
            return;
        } else {
            log_trace("Quest lottery: Quest %d failed probability roll, continuing to next quest", entry->quest_id);
        }
    }
    
    /* No quest won the lottery */
    log_trace("Quest lottery: === LOTTERY END === No quest won - level remains quest-free");
}

/* Function to reset pending quest state changes */
static void reset_pending_quest_states(void) {
    pending_quest_states.has_aule_change = false;
    pending_quest_states.has_mandos_change = false;
    pending_quest_states.has_varda_change = false;
    pending_quest_states.varda_level = 0;
    pending_quest_states.varda_vault_y = 0;
    pending_quest_states.varda_vault_x = 0;
    
    /* Reset quest lottery for new level */
    quest_lottery_winner = 0;
    quest_lottery_resolved = false;
    
    log_trace("Quest lottery: Reset for new level generation");
}

/* Function to reset quest states that were set by quest vaults during regeneration */
static void reset_quest_vault_states(void) {
    /* Only reset quest states if they were set at the current level during quest vault placement */
    /* This prevents interfering with quests that were legitimately started on other levels */
    
    log_trace("Quest vault regeneration: START - depth=%d, quest_reserved[0]=%d", 
              p_ptr->depth, p_ptr->quest_reserved[0]);
    log_trace("Quest vault regeneration: Aule state=%d level=%d, Mandos state=%d level=%d, Tulkas state=%d", 
              p_ptr->aule_quest, p_ptr->aule_level, p_ptr->mandos_quest, p_ptr->mandos_level, p_ptr->tulkas_quest);
    log_trace("Quest vault regeneration: Pending changes - aule=%s mandos=%s", 
              pending_quest_states.has_aule_change ? "yes" : "no", 
              pending_quest_states.has_mandos_change ? "yes" : "no");
    
    /* Reset vault-based quests (Aule, Mandos) */
    if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT && p_ptr->aule_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Aule quest from FORGE_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        p_ptr->aule_level = 0;
    }
    
    if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT && p_ptr->mandos_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Mandos quest from GIVER_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
        p_ptr->mandos_level = 0;
    }
    
    /* Reset entrance-based quests (Tulkas) - these don't store level but spawn during this generation */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_GIVER_PRESENT) {
        log_trace("Quest vault regeneration: Resetting Tulkas quest from GIVER_PRESENT to NOT_STARTED");
        p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
        /* Clear any Tulkas-related quest data */
        p_ptr->tulkas_target_r_idx = 0;
        p_ptr->tulkas_prize_a_idx = 0;
    }
    
    /* Reset entrance-based quests (Niena) - similar to Tulkas, spawns during generation */
    if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT) {
        log_trace("Quest vault regeneration: Resetting Niena quest from GIVER_PRESENT to NOT_STARTED");
        p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
        /* Clear Niena-related quest data */
        p_ptr->niena_monsters_seen = 0;
        p_ptr->niena_monsters_killed = 0;
    }

    /* Reset entrance-based quests (Varda) - spawns during generation on early depths */
    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT && p_ptr->varda_level == p_ptr->depth) {
        log_trace("Quest vault regeneration: Resetting Varda quest from GIVER_PRESENT to NOT_STARTED (level %d)", p_ptr->depth);
        p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        p_ptr->varda_level = 0;
    }
    
    /* CRITICAL: Preserve quest lottery result during regeneration */
    /* The lottery determines which quest (if any) owns this level and should persist */
    /* across all regeneration attempts until the quest succeeds or we abandon the level */
    
    /* Reset quest states to allow fresh placement attempts, but preserve reservation */
    bool quest_active = (quest_lottery_winner > 0) ||
                        (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED) ||
                        (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED) ||
                        (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED) ||
                        (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED) ||
                        (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED) ||
                        (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED);
    
    if (quest_active) {
        /* Keep quest_reserved[0] = 1 since a quest owns this level/run */
        if (!p_ptr->quest_reserved[0]) {
            p_ptr->quest_reserved[0] = 1;
            log_trace("Quest vault regeneration: Quest context active (lottery=%d) - ensuring quest_reserved[0] = 1", quest_lottery_winner);
        }
    } else if (p_ptr->quest_reserved[0]) {
        log_trace("Quest vault regeneration: No quest owns this level - resetting quest_reserved[0] from 1 to 0");
        p_ptr->quest_reserved[0] = 0;
    }
    
    log_trace("Quest vault regeneration: END - quest_reserved[0]=%d, lottery_winner=%d", 
              p_ptr->quest_reserved[0], quest_lottery_winner);
}

/* Function to apply pending quest state changes when level generation is successful */
static void apply_pending_quest_states(void) {
    if (pending_quest_states.has_aule_change) {
        p_ptr->aule_level = pending_quest_states.aule_level;
        p_ptr->aule_quest = AULE_QUEST_FORGE_PRESENT;
        p_ptr->quest_reserved[0] = 1; /* Mark that a quest has spawned this run */
        log_trace("Aule quest: FORGE_PRESENT APPLIED (deferred from quest vault) at %d,%d depth=%d", 
                  pending_quest_states.aule_forge_y, pending_quest_states.aule_forge_x, pending_quest_states.aule_level);
    }
    if (pending_quest_states.has_mandos_change) {
        p_ptr->mandos_level = pending_quest_states.mandos_level;
        p_ptr->mandos_quest = MANDOS_QUEST_GIVER_PRESENT;
        p_ptr->quest_reserved[0] = 1; /* Mark that a quest has spawned this run */
        log_trace("Mandos quest: GIVER_PRESENT APPLIED (deferred from quest vault) at %d,%d depth=%d", 
                  pending_quest_states.mandos_vault_y, pending_quest_states.mandos_vault_x, pending_quest_states.mandos_level);
    }
    if (pending_quest_states.has_varda_change) {
        p_ptr->varda_level = pending_quest_states.varda_level;
        p_ptr->varda_vault_placed = 1;
        p_ptr->varda_vault_ready = 0;
        p_ptr->quest_reserved[0] = 1; /* Mark that a quest has spawned this run */
        log_trace("Varda quest: Bastion placement APPLIED (deferred) at %d,%d depth=%d", 
                  pending_quest_states.varda_vault_y, pending_quest_states.varda_vault_x, pending_quest_states.varda_level);
    }
    
    /* Reset pending changes after applying them */
    reset_pending_quest_states();
}

/*
 * Note that Level generation is *not* an important bottleneck,
 * though it can be annoyingly slow on older machines...  Thus
 * we emphasize "simplicity" and "correctness" over "speed".
 *
 * This entire file is only needed for generating levels.
 * This may allow smart compilers to only load it when needed.
 *
 * Consider the "vault.txt" file for vault generation.
 *
 * In this file, we use the "special" granite and perma-wall sub-types,
 * where "basic" is normal, "inner" is inside a room, "outer" is the
 * outer wall of a room, and "solid" is the outer wall of the dungeon
 * or any walls that may not be pierced by corridors.
 *
 * Note that the cave grid flags changed in a rather drastic manner
 * for Angband 2.8.0 (and 2.7.9+), in particular, dungeon terrain
 * features, such as doors and stairs and traps and rubble and walls,
 * are all handled as a set of 64 possible "terrain features", and
 * not as "fake" objects (440-479) as in pre-2.8.0 versions.
 *
 * The 64 new "dungeon features" will also be used for "visual display"
 * but we must be careful not to allow, for example, the user to display
 * hidden traps in a different way from floors, or secret doors in a way
 * different from granite walls, or even permanent granite in a different
 * way from granite.  XXX XXX XXX
 *
 * Sil notes:
 *
 * I do not make any use of "solid" walls, but have left the type in.
 * The code previously used a lot of 11x11 blocks in room generation.
 * I have mostly removed references to this now.
 * The rooms are now placed at random in the dungeon.
 * The corridor generation has been simplified a lot for aesthetic purposes.
 * Note that level generation can fail (if the level is unconnected, or for
 * other reasons) and that each room and corridor generation can fail too. This
 * is not a problem as they are generated until success and often succeed.
 */

/*
 * Dungeon generation values
 */

#define DUN_DEST 1 /* 1/chance of having a destroyed level */

/*
 * Dungeon streamer generation values
 */
#define DUN_STR_DEN 5 /* Density of streamers */
#define DUN_STR_RNG 2 /* Width of streamers */
#define DUN_STR_QUA 4 /* Number of quartz streamers */

/*
 * Dungeon treausre allocation values
 */
#define DUN_OBJ_CHANCE_ROOM 30 /* determines number of items found in rooms */
#define DUN_OBJ_CHANCE_BOTH                                                    \
    5 /* determines number of items found in rooms/corridors */

/*
 * Hack -- Dungeon allocation "places"
 */
#define ALLOC_SET_CORR 1 /* Hallway */
#define ALLOC_SET_ROOM 2 /* Room */
#define ALLOC_SET_BOTH 3 /* Anywhere */

/*
 * Hack -- Dungeon allocation "types"
 */
#define ALLOC_TYP_RUBBLE 1 /* Rubble */
#define ALLOC_TYP_OBJECT 5 /* Object */

/*
 * Maximum numbers of rooms along each axis (currently 6x18)
 */

#define MAX_ROOMS_ROW (MAX_DUNGEON_HGT / BLOCK_HGT)
#define MAX_ROOMS_COL (MAX_DUNGEON_WID / BLOCK_WID)

/*
 * Bounds on some arrays used in the "dun_data" structure.
 * These bounds are checked, though usually this is a formality.
 */
#define CENT_MAX 110
#define DOOR_MAX 200
#define WALL_MAX 500
#define TUNN_MAX 900

bool allow_uniques;

/*
 * Maximal number of room types
 */
#define ROOM_MAX 12
#define ROOM_MIN 2

/*
 * Simple structure to hold a map location
 */

typedef struct coord coord;

struct coord
{
    byte y;
    byte x;
};

/*
 * Simple structure to hold a map location
 */

typedef struct rectangle rectangle;

struct rectangle
{
    byte y1;
    byte x1;
    byte y2;
    byte x2;
};

typedef enum room_kind
{
    ROOM_KIND_NONE = 0,
    ROOM_KIND_CLASSIC = 1,
    ROOM_KIND_CROSS = 2,
    ROOM_KIND_INTERESTING = 6,
    ROOM_KIND_LESSER_VAULT = 7,
    ROOM_KIND_GREATER_VAULT = 8
} room_kind_t;

/*
 * Structure to hold all "dungeon generation" data
 */

typedef struct dun_data dun_data;

struct dun_data
{
    /* Classifies each room slot by the builder that created it (1,2,6,7,8) */
    byte kind[CENT_MAX];
    bool is_quest[CENT_MAX];

    /* Array of centers of rooms */
    int cent_n;
    coord cent[CENT_MAX];

    /* Sil: Array of room corners */
    rectangle corner[CENT_MAX];

    /* Sil: Array of what dungeon piece each room is in */
    byte piece[CENT_MAX];

    /* Array of connections between rooms */
    bool connection[DUN_ROOMS][DUN_ROOMS];
};

/*
 * Dungeon generation data -- see "cave_gen()"
 */
static dun_data* dun;

/*
 * Array[DUNGEON_HGT][DUNGEON_WID].
 * Each corridor square it is marked for each room that it connects.
 */
int cave_corridor1[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
int cave_corridor2[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

#define LAYOUT_ANCHOR_MAX CENT_MAX

typedef enum layout_anchor_kind {
    LAYOUT_ANCHOR_NONE = 0,
    LAYOUT_ANCHOR_ROOM,
    LAYOUT_ANCHOR_PREFAB, /* seeded prefab anchor (vault/room) */
    LAYOUT_ANCHOR_CA_BLOB,
    LAYOUT_ANCHOR_BSP_SLICE,
    LAYOUT_ANCHOR_SETPIECE
} layout_anchor_kind_t;

typedef struct layout_anchor {
    layout_anchor_kind_t kind;
    rectangle bounds;
    coord center;
    byte room_kind;
    bool is_quest;
    bool requires_neighbor;
    bool neighbor_linked;
    int style_primary;
    int room_slot;
} layout_anchor_t;

static layout_anchor_t layout_anchors[LAYOUT_ANCHOR_MAX];
static int layout_anchor_count = 0;
static layout_anchor_kind_t room_anchor_kind[CENT_MAX];
static bool room_anchor_requires_neighbor[CENT_MAX];

static bool room_kind_is_vault(byte kind)
{
    return (kind >= ROOM_KIND_INTERESTING);
}

static bool area_is_basic_granite(int y1, int x1, int y2, int x2)
{
    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT || y1 < 0 || x1 < 0)
        return false;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                return false;
        }
    }
    return true;
}

/* Decode a style index from the color encoding at (y,x); returns -1 if none */
static int style_at_color(int y, int x)
{
    if (y < 0 || x < 0 || y >= MAX_DUNGEON_HGT || x >= MAX_DUNGEON_WID)
        return -1;
    return styles_decode_color_style(cave_color[y][x]);
}

static void layout_anchor_reset(void)
{
    layout_anchor_count = 0;
    for (int i = 0; i < LAYOUT_ANCHOR_MAX; ++i)
    {
        layout_anchors[i].kind = LAYOUT_ANCHOR_NONE;
        layout_anchors[i].style_primary = -1;
        layout_anchors[i].room_slot = -1;
        layout_anchors[i].requires_neighbor = false;
        layout_anchors[i].neighbor_linked = false;
    }
    for (int i = 0; i < CENT_MAX; ++i)
    {
        room_anchor_kind[i] = LAYOUT_ANCHOR_NONE;
        room_anchor_requires_neighbor[i] = false;
    }
}

static void mark_room_anchor_meta(int room_idx, layout_anchor_kind_t kind, bool requires_neighbor)
{
    if (room_idx < 0 || room_idx >= CENT_MAX)
        return;
    room_anchor_kind[room_idx] = kind;
    room_anchor_requires_neighbor[room_idx] = requires_neighbor;
}

static void layout_anchor_capture_room(int room_idx)
{
    if (layout_anchor_count >= LAYOUT_ANCHOR_MAX)
    {
        return;
    }

    layout_anchor_t* a = &layout_anchors[layout_anchor_count++];
    layout_anchor_kind_t kind = room_anchor_kind[room_idx];
    if (kind == LAYOUT_ANCHOR_NONE)
        kind = LAYOUT_ANCHOR_ROOM;
    a->kind = kind;
    a->bounds = dun->corner[room_idx];
    a->center = dun->cent[room_idx];
    a->room_kind = dun->kind[room_idx];
    a->is_quest = dun->is_quest[room_idx];
    a->style_primary = style_at_color(a->center.y, a->center.x);
    a->requires_neighbor = room_anchor_requires_neighbor[room_idx];
    a->neighbor_linked = false;
    a->room_slot = room_idx;
}

static void layout_anchor_capture_existing_rooms(void)
{
    for (int i = 0; i < dun->cent_n; ++i)
    {
        layout_anchor_capture_room(i);
    }
}

/* Forward declarations for prefab/vault builders used by anchor seeding */
static bool build_type6(int y0, int x0, bool force_forge);
static bool build_type7(int y0, int x0);
static bool build_type8(int y0, int x0);
static void seed_ca_blob_anchors(void);
static void seed_bsp_slice_anchors(void);

/* Attempt to place a prefab vault/room as a generation anchor */
static bool place_prefab_anchor_of_type(int typ, bool require_neighbor)
{
    if (dun->cent_n >= DUN_ROOMS - 1)
    {
        return false;
    }

    int y = rand_range(5, p_ptr->cur_map_hgt - 5);
    int x = rand_range(5, p_ptr->cur_map_wid - 5);
    int before = dun->cent_n;
    bool ok = false;

    switch (typ)
    {
    case 8:
        ok = build_type8(y, x);
        break;
    case 7:
        ok = build_type7(y, x);
        break;
    case 6:
    default:
        ok = build_type6(y, x, false);
        break;
    }

    if (!ok || dun->cent_n <= before)
    {
        return false;
    }

    int slot = dun->cent_n - 1;
    mark_room_anchor_meta(slot, LAYOUT_ANCHOR_PREFAB, require_neighbor);
    return true;
}

/* Seed a small number of prefab anchors up-front to diversify layouts */
static void seed_prefab_anchors(void)
{
    int target = 1;
    if (p_ptr->depth >= 15)
        target++;
    if (p_ptr->depth >= 30 && one_in_(2))
        target++;

    int placed = 0;
    int attempts = 0;
    int max_attempts = target * 6;

    while (placed < target && attempts < max_attempts)
    {
        attempts++;
        /* Bias deeper levels toward larger prefabs, shallow toward type6 */
        int roll = rand_int(100);
        int typ = 6;
        if (roll > 85 && p_ptr->depth > 25)
            typ = 8;
        else if (roll > 60 && p_ptr->depth > 10)
            typ = 7;

        /* Reserve some anchors for future adjacency setpieces */
        bool require_neighbor = one_in_(3);

        if (place_prefab_anchor_of_type(typ, require_neighbor))
        {
            placed++;
            log_trace("Prefab anchor: placed type %d (require_neighbor=%s) [placed=%d target=%d attempts=%d]",
                typ, require_neighbor ? "true" : "false", placed, target, attempts);
        }
    }

    log_trace("Prefab anchor seeding complete: placed=%d target=%d attempts=%d depth=%d",
        placed, target, attempts, p_ptr->depth);
}

/* Carve a small cellular-automata style blob and register it as an anchor */
static bool carve_ca_blob_anchor(void)
{
    if (dun->cent_n >= DUN_ROOMS - 1)
        return false;

    /* Pick blob dimensions */
    int h = rand_range(8, 16);
    int w = rand_range(10, 18);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    /* Ensure we are carving into untouched granite */
    /* Allow slight overlap with walls but not existing floors */
    if (y1 < 1 || x1 < 1 || y2 >= p_ptr->cur_map_hgt - 1 || x2 >= p_ptr->cur_map_wid - 1)
        return false;
    for (int y = y1 - 1; y <= y2 + 1; ++y)
    {
        for (int x = x1 - 1; x <= x2 + 1; ++x)
        {
            if (cave_floor_bold(y, x))
                return false;
        }
    }

    /* Simple CA grid stored on stack (max ~20x20) */
    bool grid[24][24];
    if (h > 24 || w > 24)
        return false;

    /* Seed noise */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = one_in_(2);

    /* Run a few steps */
    for (int step = 0; step < 3; ++step)
    {
        bool next[24][24];
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0)
                            continue;
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (grid[ny][nx])
                            neighbors++;
                    }
                }
                /* Standard cave CA: birth>=5 survive>=4 */
                if (grid[y][x])
                    next[y][x] = (neighbors >= 4);
                else
                    next[y][x] = (neighbors >= 5);
            }
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                grid[y][x] = next[y][x];
    }

    /* Apply to dungeon */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!grid[y][x])
                continue;
            int gy = y1 + y;
            int gx = x1 + x;
            cave_set_feat(gy, gx, FEAT_FLOOR);
            cave_info[gy][gx] |= CAVE_ROOM;
            floor_count++;
            if (gy < min_y)
                min_y = gy;
            if (gy > max_y)
                max_y = gy;
            if (gx < min_x)
                min_x = gx;
            if (gx > max_x)
                max_x = gx;
        }
    }

    if (floor_count < 10)
        return false;

    /* Pick a center on a floor tile */
    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, one_in_(3));

    log_trace("CA blob anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    return true;
}

/* Try to seed a few CA blob anchors in unused granite */
static void seed_ca_blob_anchors(void)
{
    int target = 2;
    if (p_ptr->depth >= 10)
        target = 3;
    if (p_ptr->depth >= 25)
        target = 4;
    int placed = 0;
    for (int attempt = 0; attempt < 36 && placed < target; ++attempt)
    {
        if (carve_ca_blob_anchor())
            placed++;
    }
    log_trace("CA blob seeding complete: placed=%d target=%d attempts=%d", placed, target, 36);
}

/* Carve a BSP-style sliced region into rooms-like rectangles and register anchor */
static bool carve_bsp_slice_anchor(void)
{
    if (dun->cent_n >= DUN_ROOMS - 1)
        return false;

    int h = rand_range(10, 18);
    int w = rand_range(12, 24);
    int y1 = rand_range(3, p_ptr->cur_map_hgt - h - 3);
    int x1 = rand_range(3, p_ptr->cur_map_wid - w - 3);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (!area_is_basic_granite(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
        return false;

    typedef struct {
        int y1, x1, y2, x2;
    } slice_rect;

    slice_rect rects[12];
    int rect_count = 1;
    rects[0].y1 = y1;
    rects[0].x1 = x1;
    rects[0].y2 = y2;
    rects[0].x2 = x2;

    int splits = rand_range(2, 4);
    for (int s = 0; s < splits && rect_count < 12; ++s)
    {
        int pick = rand_int(rect_count);
        slice_rect r = rects[pick];
        int rw = r.x2 - r.x1 + 1;
        int rh = r.y2 - r.y1 + 1;
        bool vertical = (rw > rh) ? true : (rh > rw ? false : one_in_(2));

        if (vertical && rw > 10)
        {
            int cut = rand_range(r.x1 + rw / 3, r.x2 - rw / 3);
            slice_rect a = {r.y1, r.x1, r.y2, cut};
            slice_rect b = {r.y1, cut + 1, r.y2, r.x2};
            if ((a.x2 - a.x1) >= 5 && (b.x2 - b.x1) >= 5)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
        else if (!vertical && rh > 8)
        {
            int cut = rand_range(r.y1 + rh / 3, r.y2 - rh / 3);
            slice_rect a = {r.y1, r.x1, cut, r.x2};
            slice_rect b = {cut + 1, r.x1, r.y2, r.x2};
            if ((a.y2 - a.y1) >= 4 && (b.y2 - b.y1) >= 4)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    for (int i = 0; i < rect_count; ++i)
    {
        slice_rect *r = &rects[i];
        for (int y = r->y1 + 1; y < r->y2; ++y)
        {
            for (int x = r->x1 + 1; x < r->x2; ++x)
            {
                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] |= CAVE_ROOM;
                floor_count++;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }

    if (floor_count < 20)
        return false;

    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty;
            cx = tx;
            break;
        }
    }

    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, one_in_(4));

    log_trace("BSP slice anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d) rects=%d",
        floor_count, min_y, min_x, max_y, max_x, cy, cx, rect_count);
    return true;
}

/* Try to seed BSP-sliced anchors in spare granite */
static void seed_bsp_slice_anchors(void)
{
    int target = (p_ptr->depth >= 8) ? 1 : 0;
    if (p_ptr->depth >= 20)
        target++;
    int placed = 0;
    for (int attempt = 0; attempt < 16 && placed < target; ++attempt)
    {
        if (carve_bsp_slice_anchor())
            placed++;
    }
    log_trace("BSP slice seeding complete: placed=%d target=%d", placed, target);
}

static bool feature_is_any_door(int feat)
{
    return (feat == FEAT_SECRET) || (feat == FEAT_OPEN) || (feat == FEAT_BROKEN)
        || ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL));
}

/* Collapse adjacent doors outside vaults to avoid double-door seams */
static int squash_double_doors(void)
{
    int removed = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!feature_is_any_door(cave_feat[y][x])) continue;
            if (cave_info[y][x] & (CAVE_ICKY)) continue;

            /* Only clear east/south neighbors to keep at most one door */
            int ny = y, nx = x + 1;
            if ((nx < p_ptr->cur_map_wid - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
            ny = y + 1; nx = x;
            if ((ny < p_ptr->cur_map_hgt - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
        }
    }
    log_trace("squash_double_doors: converted %d adjacent doors to floor", removed);
    return removed;
}

/* determines whether the player can pass through a given feature */
/* icky locations (inside vaults) are all considered passable.    */
bool player_passable(int y, int x, bool ignore_rubble_and_chasms)
{
    byte feature = cave_feat[y][x];
    bool icky_interior = (cave_info[y][x] & (CAVE_ICKY))
        && (cave_info[y][x - 1] & (CAVE_ICKY))
        && (cave_info[y][x + 1] & (CAVE_ICKY))
        && (cave_info[y - 1][x] & (CAVE_ICKY))
        && (cave_info[y + 1][x] & (CAVE_ICKY));

    if ((feature < FEAT_WALL_HEAD) || (feature > FEAT_WALL_TAIL))
    {
        return !((feature == FEAT_CHASM) && !ignore_rubble_and_chasms);
    }
    else
    {
        return (feature == FEAT_SECRET)
            || ((feature == FEAT_RUBBLE) && ignore_rubble_and_chasms)
            || icky_interior;
    }
}

/* floodfills access through the dungeon, marking all accessible squares with
 * true */
void flood_access(int y, int x,
    int access_array[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    bool ignore_rubble_and_chasms)
{
    /* first check the map bounds */
    if ((y < 0) || (y > p_ptr->cur_map_hgt) || (x < 0)
        || (x > p_ptr->cur_map_wid))
        return;

    access_array[y][x] = true;
    if (player_passable(y - 1, x - 1, ignore_rubble_and_chasms)
        && (access_array[y - 1][x - 1] == false))
        flood_access(y - 1, x - 1, access_array, ignore_rubble_and_chasms);
    if (player_passable(y - 1, x, ignore_rubble_and_chasms)
        && (access_array[y - 1][x] == false))
        flood_access(y - 1, x, access_array, ignore_rubble_and_chasms);
    if (player_passable(y - 1, x + 1, ignore_rubble_and_chasms)
        && (access_array[y - 1][x + 1] == false))
        flood_access(y - 1, x + 1, access_array, ignore_rubble_and_chasms);
    if (player_passable(y, x - 1, ignore_rubble_and_chasms)
        && (access_array[y][x - 1] == false))
        flood_access(y, x - 1, access_array, ignore_rubble_and_chasms);
    if (player_passable(y, x + 1, ignore_rubble_and_chasms)
        && (access_array[y][x + 1] == false))
        flood_access(y, x + 1, access_array, ignore_rubble_and_chasms);
    if (player_passable(y + 1, x - 1, ignore_rubble_and_chasms)
        && (access_array[y + 1][x - 1] == false))
        flood_access(y + 1, x - 1, access_array, ignore_rubble_and_chasms);
    if (player_passable(y + 1, x, ignore_rubble_and_chasms)
        && (access_array[y + 1][x] == false))
        flood_access(y + 1, x, access_array, ignore_rubble_and_chasms);
    if (player_passable(y + 1, x + 1, ignore_rubble_and_chasms)
        && (access_array[y + 1][x + 1] == false))
        flood_access(y + 1, x + 1, access_array, ignore_rubble_and_chasms);
    return;
}

void label_rooms(void)
{
    int i;

    for (i = 0; i < dun->cent_n; i++)
    {
        // cave_feat[dun->corner[i].y1][dun->corner[i].x1] = 5 + 1;
        // cave_feat[dun->corner[i].y1][dun->corner[i].x2] = 5 + 1;
        // cave_feat[dun->corner[i].y2][dun->corner[i].x1] = 5 + 1;
        // cave_feat[dun->corner[i].y2][dun->corner[i].x2] = 5 + 1;

        cave_feat[dun->cent[i].y][dun->cent[i].x] = 5 + (i % 10);
        if (i > 9)
        {
            cave_feat[dun->cent[i].y][dun->cent[i].x - 1] = 5 + ((i / 10) % 10);
        }
        if (i > 99)
        {
            cave_feat[dun->cent[i].y][dun->cent[i].x - 1]
                = 5 + ((i / 100) % 10);
        }
    }
}

// floodfills access through the *graph* of the dungeon
void flood_piece(int n, int piece_num)
{
    int i;

    dun->piece[n] = piece_num;

    for (i = 0; i < dun->cent_n; i++)
    {
        if (dun->connection[n][i] && (dun->piece[i] == 0))
        {
            flood_piece(i, piece_num);
        }
    }
    return;
}

int dungeon_pieces(void)
{
    int piece_num;
    int i;

    // first reset the pieces
    for (i = 0; i < dun->cent_n; i++)
    {
        dun->piece[i] = 0;
    }

    for (piece_num = 1; piece_num <= dun->cent_n; piece_num++)
    {
        // find the next room that doesn't belong to a piece
        for (i = 0; (i < dun->cent_n) && (dun->piece[i] != 0); i++)
            ;

        if (i == dun->cent_n)
        {
            break;
        }
        else
        {
            flood_piece(i, piece_num);
        }
    }

    return (piece_num - 1);
}

/*
 * Convert existing terrain type to rubble
 */
static void place_rubble(int y, int x)
{
    /* Create rubble */
    if (p_ptr->depth >= 4 && cave_feat[y][x] != FEAT_MORE
        && cave_feat[y][x] != FEAT_LESS)
        cave_set_feat(y, x, FEAT_RUBBLE);
}

/*
 * Choose either an ordinary up staircase or an up shaft.
 */
static int choose_up_stairs(void)
{
    if (p_ptr->depth >= 2)
    {
        if (one_in_(2) || p_ptr->on_the_run)
            return (FEAT_LESS_SHAFT);
    }

    return (FEAT_LESS);
}

/*
 * Choose either an ordinary down staircase or an down shaft.
 */
static int choose_down_stairs(void)
{
    if (p_ptr->depth < MORGOTH_DEPTH - 2)
    {
        if (one_in_(2) || p_ptr->on_the_run)
            return (FEAT_MORE_SHAFT);
    }

    return (FEAT_MORE);
}

/*
 * Calculate the minimum distance between any up stairs and any down stairs on the level.
 * Returns -1 if either type of stairs is not found.
 */
static int calculate_min_stair_distance(void)
{
    int min_distance = 9999;
    bool found_up = false;
    bool found_down = false;
    
    /* Find all up and down stairs and calculate minimum distance */
    for (int y1 = 0; y1 < p_ptr->cur_map_hgt; y1++)
    {
        for (int x1 = 0; x1 < p_ptr->cur_map_wid; x1++)
        {
            /* Check if this is an up stair */
            if (cave_feat[y1][x1] == FEAT_LESS || cave_feat[y1][x1] == FEAT_LESS_SHAFT)
            {
                found_up = true;
                
                /* Check distance to all down stairs */
                for (int y2 = 0; y2 < p_ptr->cur_map_hgt; y2++)
                {
                    for (int x2 = 0; x2 < p_ptr->cur_map_wid; x2++)
                    {
                        if (cave_feat[y2][x2] == FEAT_MORE || cave_feat[y2][x2] == FEAT_MORE_SHAFT)
                        {
                            found_down = true;
                            int dist = distance(y1, x1, y2, x2);
                            if (dist < min_distance)
                            {
                                min_distance = dist;
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* Return -1 if we didn't find both types of stairs */
    if (!found_up || !found_down)
    {
        return -1;
    }
    
    return min_distance;
}

/*
 * Place an up/down staircase at given location
 */
void place_random_stairs(int y, int x)
{
    /* Paranoia */
    if (!cave_clean_bold(y, x))
        return;

    /* Create a staircase */
    if (!p_ptr->depth)
    {
        cave_set_feat(y, x, FEAT_MORE);
    }
    else if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        if (one_in_(2))
            cave_set_feat(y, x, FEAT_LESS);
        else
            cave_set_feat(y, x, FEAT_LESS_SHAFT);
    }
    else if (one_in_(2))
    {
        if (p_ptr->depth <= 1)
            cave_set_feat(y, x, FEAT_MORE);
        else if (one_in_(2))
            cave_set_feat(y, x, FEAT_MORE);
        else
            cave_set_feat(y, x, FEAT_MORE_SHAFT);
    }
    else
    {
        if ((one_in_(2)) || (p_ptr->depth == 1))
            cave_set_feat(y, x, FEAT_LESS);
        else
            cave_set_feat(y, x, FEAT_LESS_SHAFT);
    }
}

static bool wearable_p(const object_type *o_ptr)
{
    /* INVEN_WIELD is the first equipment slot (see defines.h)           */
    /* Anything that gets a slot number below that lives in inventory.    */
    return (wield_slot(o_ptr) >= INVEN_WIELD);
}

/*
 * Generate the chosen item at a random spot in the dungeon.
 * If 'close' is true, it must be nearby and in line-of-sight of the player.
 */
void place_item_randomly(int tval, int sval, bool close)
{
    object_type* i_ptr;
    object_type object_type_body;
    int y, x;
    int i;
    s16b k_idx;

    if (close)
    {
        for (i = 0; i < 1000; i++)
        {
            y = p_ptr->py + rand_range(-5, +5);
            x = p_ptr->px + rand_range(-5, +5);

            if (cave_naked_bold(y, x) && los(p_ptr->py, p_ptr->px, y, x)
                && (cave_info[y][x] & (CAVE_ROOM)))
            {
                break;
            }
        }
    }
    else
    {
        for (i = 0; i < 1000; i++)
        {
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            if (cave_naked_bold(y, x))
            {
                break;
            }
        }
    }

    /* Get local object */
    i_ptr = &object_type_body;

    /* Get the object_kind */
    k_idx = lookup_kind(tval, sval);

    /* Valid item? */
    if (!k_idx)
        return;

    /* Paranoia regarding having found a spot */
    if (i == 1000)
        return;

    /* Prepare the item */
    object_prep(i_ptr, k_idx);

    /* Escape-curse: higher chance of cursed finds */
    {
        int stacks = curse_flag_count_cur(CUR_FINDCURSE);
        if (stacks && wearable_p(i_ptr))
        {
            int chance = 20 >> stacks;         /* base 1-in-20 ÔåÆ 1-in-10 ÔåÆ 1-in-5 */
            if (!chance || one_in_(chance))
                add_random_curse(i_ptr);
        }
    }


    if (tval == TV_ARROW)
    {
        i_ptr->number = (byte)24;
    }
    else
    {
        i_ptr->number = (byte)1;
    }

    drop_near(i_ptr, 0, y, x);
}

/*
 * Allocates some objects (using "place" and "type")
 */
static void alloc_object(int set, int typ, int num, bool out_of_sight)
{
    int y, x, k, i;

    /* Place some objects */
    for (k = 0; k < num; k++)
    {
        /* Pick a "legal" spot */
        for (i = 0; i < 10000; i++)
        {
            bool is_room;

            /* Location */
            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            /* Require "naked" floor grid */
            if (!cave_naked_bold(y, x))
                continue;

            /* Check for "room" */
            is_room = (cave_info[y][x] & (CAVE_ROOM)) ? true : false;

            /* Require corridor? */
            if ((set == ALLOC_SET_CORR) && is_room)
                continue;

            /* Require room? */
            if ((set == ALLOC_SET_ROOM) && !is_room)
                continue;

            /* Require out_of_sight -- actually more than MAX_SIGHT squares away
             */
            if (out_of_sight
                && (distance(p_ptr->py, p_ptr->px, y, x) <= MAX_SIGHT))
                continue;

            /* Accept it */
            break;
        }

        /* No point found */
        if (i == 10000)
            return;

        /* Place something */
        switch (typ)
        {
        case ALLOC_TYP_RUBBLE:
        {
            place_rubble(y, x);
            break;
        }

        case ALLOC_TYP_OBJECT:
        {
            place_object(y, x, false, false, DROP_TYPE_UNTHEMED);
            break;
        }
        }
    }
}

/*
 * Places "streamers" of quartz through dungeon
 */
static bool build_streamer(int feat)
{
    int i, tx, ty;
    int y, x, dir;
    int tries1 = 0;
    int tries2 = 0;

    /* Hack -- Choose starting point */
    y = rand_spread(p_ptr->cur_map_hgt / 2, 10);
    x = rand_spread(p_ptr->cur_map_wid / 2, 15);

    /* Choose a random compass direction */
    dir = ddd[rand_int(8)];

    /* Place streamer into dungeon */
    while (true)
    {
        tries1++;

        if (tries1 > 2500)
            return (false);

        /* One grid per density */
        for (i = 0; i < DUN_STR_DEN; i++)
        {
            int d = DUN_STR_RNG;

            /* Pick a nearby grid */
            while (true)
            {
                tries2++;
                if (tries2 > 2500)
                    return (false);
                ty = rand_spread(y, d);
                tx = rand_spread(x, d);
                if (!in_bounds(ty, tx))
                    continue;
                break;
            }

            /* Only convert "granite" walls */
            if (cave_feat[ty][tx] < FEAT_WALL_EXTRA)
                continue;
            if (cave_feat[ty][tx] > FEAT_WALL_SOLID)
                continue;

            /* Clear previous contents, add proper vein type */
            cave_set_feat(ty, tx, feat);
        }

        /* Advance the streamer */
        y += ddy[dir];
        x += ddx[dir];

        /* Stop at dungeon edge */
        if (!in_bounds(y, x))
            break;
    }

    return (true);
}

/*
 * Places a single chasm
 */
static bool build_chasm(void)
{
    int i;
    int y, x;
    int main_dir, new_dir;
    int length;
    int floor_to_chasm;

    bool chasm_ok = false;

    while (!chasm_ok)
    {
        // choose starting point
        y = rand_range(10, p_ptr->cur_map_hgt - 10);
        x = rand_range(10, p_ptr->cur_map_wid - 10);

        // choose a random cardinal direction for it to run in
        main_dir = ddd[rand_int(4)];

        // choose a random length for it
        length = damroll(4, 8);

        // determine its shape
        for (i = 0; i < length; i++)
        {
            // go in a random direction half the time
            if (one_in_(2))
            {
                // choose the random cardinal direction
                new_dir = ddd[rand_int(4)];
                y += ddy[new_dir];
                x += ddx[new_dir];
            }

            // go straight ahead the other half
            else
            {
                y += ddy[main_dir];
                x += ddx[main_dir];
            }

            // stop near dungeon edge
            if ((y < 3) || (y > p_ptr->cur_map_hgt - 3) || (x < 3)
                || (x > p_ptr->cur_map_wid - 3))
                break;

            // mark that we want to put a chasm here
            cave_info[y][x] |= (CAVE_TEMP);
        }

        // start by assuming it will be OK
        chasm_ok = true;

        // count floor squares that will be turned to chasm
        floor_to_chasm = 0;

        // check it doesn't wreck the dungeon
        for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        {
            for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
            {
                // only inspect squares that are currently destined to be chasms
                if (cave_info[y][x] & (CAVE_TEMP))
                {
                    // avoid chasms in interesting rooms / vaults
                    if (cave_info[y][x] & (CAVE_ICKY))
                    {
                        chasm_ok = false;
                    }

                    // avoid two chasm square in a row in corridors
                    if ((cave_info[y + 1][x] & (CAVE_TEMP))
                        && !(cave_info[y][x] & (CAVE_ROOM))
                        && !(cave_info[y + 1][x] & (CAVE_ROOM))
                        && cave_floorlike_bold(y, x)
                        && cave_floorlike_bold(y + 1, x))
                    {
                        chasm_ok = false;
                    }
                    if ((cave_info[y][x + 1] & (CAVE_TEMP))
                        && !(cave_info[y][x] & (CAVE_ROOM))
                        && !(cave_info[y][x + 1] & (CAVE_ROOM))
                        && cave_floorlike_bold(y, x)
                        && cave_floorlike_bold(y, x + 1))
                    {
                        chasm_ok = false;
                    }

                    // avoid a chasm taking out the rock next to a door
                    if (cave_any_closed_door_bold(y + 1, x)
                        || cave_any_closed_door_bold(y - 1, x)
                        || cave_any_closed_door_bold(y, x + 1)
                        || cave_any_closed_door_bold(y, x - 1))
                    {
                        chasm_ok = false;
                    }

                    // avoid a chasm just hitting the wall of a lit room (would
                    // look odd that the light doesn't hit the wall behind)
                    if (cave_wall_bold(y, x) && (cave_info[y][x] & (CAVE_GLOW)))
                    {
                        if ((cave_wall_bold(y + 1, x)
                                && !(cave_info[y + 1][x] & (CAVE_GLOW))
                                && !(cave_info[y + 1][x] & (CAVE_TEMP)))
                            || (cave_wall_bold(y - 1, x)
                                && !(cave_info[y - 1][x] & (CAVE_GLOW))
                                && !(cave_info[y - 1][x] & (CAVE_TEMP)))
                            || (cave_wall_bold(y, x + 1)
                                && !(cave_info[y][x + 1] & (CAVE_GLOW))
                                && !(cave_info[y][x + 1] & (CAVE_TEMP)))
                            || (cave_wall_bold(y, x - 1)
                                && !(cave_info[y][x - 1] & (CAVE_GLOW))
                                && !(cave_info[y][x - 1] & (CAVE_TEMP))))
                        {
                            chasm_ok = false;
                        }
                    }

                    // avoid a chasm having no squares in a room/corridor
                    if (cave_floor_bold(y, x))
                    {
                        floor_to_chasm++;
                    }
                }
            }
        }

        // the chasm must affect at least one floor square
        if (floor_to_chasm < 1)
            chasm_ok = false;

        // clear the flag for failed chasm placement
        if (!chasm_ok)
        {
            for (y = 0; y < p_ptr->cur_map_hgt; y++)
            {
                for (x = 0; x < p_ptr->cur_map_wid; x++)
                {
                    if (cave_info[y][x] & (CAVE_TEMP))
                    {
                        cave_info[y][x] &= ~(CAVE_TEMP);
                    }
                }
            }
        }
    }

    // actually place the chasm and clear the flag
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_info[y][x] & (CAVE_TEMP))
            {
                cave_set_feat(y, x, FEAT_CHASM);
                cave_info[y][x] &= ~(CAVE_TEMP);
            }
        }
    }

    return (true);
}

/*
 * Places chasms through dungeon
 */
static void build_chasms(void)
{
    int i;
    int chasms = 0;
    int panels = (p_ptr->cur_map_hgt / PANEL_HGT)
        * (p_ptr->cur_map_wid / PANEL_WID_FIXED);

    // determine whether to add chasms, and how many
    if ((p_ptr->depth > 2) && (p_ptr->depth < MORGOTH_DEPTH - 1)
        && percent_chance(p_ptr->depth + 30))
    {
        // add some chasms
        chasms += damroll(1, panels / 3);

        // flip a coin, and if it is heads...
        while (one_in_(2))
        {
            // add some more chasms and flip again...
            chasms += damroll(1, panels / 3);
        }
    }

    if (chasms > 12)
        chasms = 12;

    // build them
    for (i = 0; i < chasms; i++)
    {
        build_chasm();
    }

    if (cheat_room && (chasms > 0))
        msg_format("%d chasms.", chasms);
}

/*
 * Generate helper -- test a rectangle to see if it is all rock (i.e. not floor
 * and not icky)
 */
static bool solid_rock(int y1, int x1, int y2, int x2)
{
    int y, x;

    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT)
        return (false);

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
                return (false);
            if (cave_info[y][x] & CAVE_ICKY)
                return (false);
        }
    }
    return (true);
}

/*
 * Sil
 * Generate helper -- test around a rectangle to see if there would be a doubled
 * wall
 *
 * eg:
 *       ######
 * #######....#
 * #....##....#
 * #....#######
 * ######
 */
static bool doubled_wall(int y1, int x1, int y2, int x2)
{
    int y, x;

    /* check top wall */
    for (x = x1; x < x2; x++)
    {
        if ((cave_feat[y1 - 2][x] == FEAT_WALL_OUTER)
            && (cave_feat[y1 - 2][x + 1] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check bottom wall */
    for (x = x1; x < x2; x++)
    {
        if ((cave_feat[y2 + 2][x] == FEAT_WALL_OUTER)
            && (cave_feat[y2 + 2][x + 1] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check left wall */
    for (y = y1; y < y2; y++)
    {
        if ((cave_feat[y][x1 - 2] == FEAT_WALL_OUTER)
            && (cave_feat[y + 1][x1 - 2] == FEAT_WALL_OUTER))
            return (true);
    }

    /* check right wall */
    for (y = y1; y < y2; y++)
    {
        if ((cave_feat[y][x2 + 2] == FEAT_WALL_OUTER)
            && (cave_feat[y + 1][x2 + 2] == FEAT_WALL_OUTER))
            return (true);
    }

    return (false);
}

/*
 * Generate helper -- create a new room with optional light
 */
static void generate_room(int y1, int x1, int y2, int x2, int light)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            cave_info[y][x] |= (CAVE_ROOM);
            if (light)
                cave_info[y][x] |= (CAVE_GLOW);
        }
    }
}

/*
 * Generate helper -- fill a rectangle with a feature
 */
static void generate_fill(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            cave_set_feat(y, x, feat);
        }
    }
}

/*
 * Generate helper -- draw a rectangle with a feature
 */
static void generate_draw(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;

    for (y = y1; y <= y2; y++)
    {
        cave_set_feat(y, x1, feat);
        cave_set_feat(y, x2, feat);
    }

    for (x = x1; x <= x2; x++)
    {
        cave_set_feat(y1, x, feat);
        cave_set_feat(y2, x, feat);
    }
}

/*
 * Generate helper -- split a rectangle with a feature
 */
static void generate_plus(int y1, int x1, int y2, int x2, int feat)
{
    int y, x;
    int y0, x0;

    /* Center */
    y0 = (y1 + y2) / 2;
    x0 = (x1 + x2) / 2;

    for (y = y1; y <= y2; y++)
    {
        cave_set_feat(y, x0, feat);
    }

    for (x = x1; x <= x2; x++)
    {
        cave_set_feat(y0, x, feat);
    }
}

static bool h_tunnel_ok(
    int x1, int x2, int y, bool tentative, int desired_changes)
{
    int x, x_lo, x_hi, changes;

    x_lo = MIN(x1, x2);
    x_hi = MAX(x1, x2);
    changes = 0;

    /* Don't dig corridors ending at a room's outer wall (can happen at corners
     * of L-corridors) */
    if ((cave_feat[y][x1] == FEAT_WALL_OUTER)
        || (cave_feat[y][x2] == FEAT_WALL_OUTER))
        return (false);
    /* Don't dig L-corridors when the corner is too close to empty space */
    if (!(cave_info[y][x_lo] & (CAVE_ROOM)))
    {
        if ((cave_feat[y - 1][x_lo - 1] == FEAT_FLOOR)
            || (cave_feat[y + 1][x_lo - 1] == FEAT_FLOOR))
        {
            return (false);
        }
    }
    if (!(cave_info[y][x_hi] & (CAVE_ROOM)))
    {
        if ((cave_feat[y - 1][x_hi + 1] == FEAT_FLOOR)
            || (cave_feat[y + 1][x_hi + 1] == FEAT_FLOOR))
        {
            return (false);
        }
    }

    /* test each location in the corridor */
    for (x = x_lo; x <= x_hi; x++)
    {
        /* count the number of times it enters or leaves a room */
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && // to outside
            (cave_floor_bold(y, x - 1)
                || (cave_feat[y][x - 1] == FEAT_WALL_INNER))) // from inside
        {
            changes++;
        }
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && // from outside
            (cave_floor_bold(y, x)
                || (cave_feat[y][x] == FEAT_WALL_INNER))) // to inside
        {
            changes++;
        }

        /* abort if the tunnel would go through two adjacent squares of the
         * outside wall of a room */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_OUTER))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall to a door */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }
        /* abort if the tunnel would go from a door to an outside wall */
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x - 1] == FEAT_DOOR_HEAD))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall into an inside wall
         */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_INNER))
        {
            return (false);
        }
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x - 1] == FEAT_WALL_INNER))
        {
            return (false);
        }

        /* abort if the tunnel would directly enter a vault without going
         * through a designated square */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x - 1)
                || (cave_feat[y][x - 1] == FEAT_WALL_INNER)))
        {
            return (false);
        }

        /* abort if the tunnel would go through or adjacent to an existing door
         * (except in vaults) */
        if (cave_known_closed_door_bold(y - 1, x)
            && !(cave_info[y - 1][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y + 1, x)
            && !(cave_info[y + 1][x] & (CAVE_ICKY)))
        {
            return (false);
        }

        /* abort if the tunnel would have floor beside it at some point outside
         * a room */
        if (((cave_feat[y + 1][x] == FEAT_FLOOR)
                || (cave_feat[y - 1][x] == FEAT_FLOOR))
            && !(cave_info[y][x] & (CAVE_ROOM)))
        {
            return (false);
        }
    }
    if (tentative && (changes != desired_changes))
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

static bool v_tunnel_ok(
    int y1, int y2, int x, bool tentative, int desired_changes)
{
    int y, y_lo, y_hi, changes;

    y_lo = MIN(y1, y2);
    y_hi = MAX(y1, y2);
    changes = 0;

    /* Don't dig corridors ending at a room's outer wall (can happen at corners
     * of L-corridors) */
    if ((cave_feat[y1][x] == FEAT_WALL_OUTER)
        || (cave_feat[y2][x] == FEAT_WALL_OUTER))
        return (false);
    /* Don't dig L-corridors when the corner is too close to empty space */
    if (!(cave_info[y_lo][x] & (CAVE_ROOM)))
    {
        if ((cave_feat[y_lo - 1][x - 1] == FEAT_FLOOR)
            || (cave_feat[y_lo - 1][x + 1] == FEAT_FLOOR))
        {
            return (false);
        }
    }
    if (!(cave_info[y_hi][x] & (CAVE_ROOM)))
    {
        if ((cave_feat[y_hi + 1][x - 1] == FEAT_FLOOR)
            || (cave_feat[y_hi + 1][x + 1] == FEAT_FLOOR))
        {
            return (false);
        }
    }

    /* test each location in the corridor */
    for (y = y_lo; y <= y_hi; y++)
    {
        /* count the number of times it enters or leaves a room */
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_floor_bold(y - 1, x)
                || (cave_feat[y - 1][x] == FEAT_WALL_INNER)))
        {
            changes++;
        }
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            changes++;
        }

        /* abort if the tunnel would go through two adjacent squares of the
         * outside wall of a room */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_OUTER))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall to a door */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }
        /* abort if the tunnel would go from a door to an outside wall */
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y - 1][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall into an inside wall
         */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_INNER))
        {
            return (false);
        }
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y - 1][x] == FEAT_WALL_INNER))
        {
            return (false);
        }

        /* abort if the tunnel would directly enter a vault without going
         * through a designated square */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y - 1, x)
                || (cave_feat[y - 1][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }

        /* abort if the tunnel would go through, or adjacent to an existing
         * (non-vault) door */
        if (cave_known_closed_door_bold(y, x - 1)
            && !(cave_info[y][x - 1] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x + 1)
            && !(cave_info[y][x + 1] & (CAVE_ICKY)))
        {
            return (false);
        }

        /* abort if the tunnel would have floor beside it at some point outside
         * a room */
        if (((cave_feat[y][x + 1] == FEAT_FLOOR)
                || (cave_feat[y][x - 1] == FEAT_FLOOR))
            && !(cave_info[y][x] & (CAVE_ROOM)))
        {
            return (false);
        }
    }
    if (tentative && (changes != desired_changes))
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

typedef enum {
    TUNNEL_TREAT_NONE = 0,
    TUNNEL_TREAT_NICHES,
    TUNNEL_TREAT_PILLARS
} tunnel_treatment;

typedef struct tunnel_profile {
    byte width;          /* 1 = normal, 2 = offset double, 3 = grand hall */
    int side_bias;       /* -1/0/1: which side to favour when width == 2 */
    tunnel_treatment treatment;
} tunnel_profile;

static const tunnel_profile TUNNEL_PROFILE_NORMAL = {1, 0, TUNNEL_TREAT_NONE};

static tunnel_profile choose_tunnel_profile(bool tentative)
{
    tunnel_profile profile = TUNNEL_PROFILE_NORMAL;

    /* Keep early levels tight and readable */
    if (p_ptr->depth < 7)
        return profile;

    /* On shallow branches, fall back to narrow connectors */
    if (tentative)
        ; /* allow style variation even on tentative digs */

    int depth = p_ptr->depth;
    int sidx = styles_get_level_primary_style();
    byte style_group = (sidx >= 0 && style_info) ? style_info[sidx].group : 0;
    bool style_grand = (style_group >= 4); /* warmer/darker palettes get a bump */

    /* Medium halls show up occasionally once the dungeon opens up */
    int medium_rarity = style_grand ? 9 : 13;    /* lower is more common */
    int grand_rarity = style_grand ? 14 : 20;
    if (depth >= 20)
    {
        medium_rarity = style_grand ? 6 : 9;
        grand_rarity = style_grand ? 9 : 14;
    }
    else if (depth >= 12)
    {
        medium_rarity = style_grand ? 8 : 12;
        grand_rarity = style_grand ? 13 : 20;
    }

    if ((depth >= 12) && one_in_(grand_rarity))
    {
        profile.width = 3;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_PILLARS : TUNNEL_TREAT_NICHES;
    }
    else if ((depth >= 10) && one_in_(medium_rarity))
    {
        profile.width = one_in_(4) ? 3 : 2;
        profile.side_bias = one_in_(2) ? 1 : -1;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_NICHES : TUNNEL_TREAT_NONE;
    }

    return profile;
}

static void apply_v_tunnel_treatment(
    int r1, int r2, int y_lo, int y_hi, int x, bool widen_west, bool widen_east,
    const tunnel_profile* profile)
{
    if (!profile)
        return;

    /* Side niches sit just outside the carved width */
    if (profile->treatment == TUNNEL_TREAT_NICHES)
    {
        int offset = (profile->width >= 3) ? 2 : 1;
        int side = 0;
        if (widen_west && widen_east)
            side = one_in_(2) ? -offset : offset;
        else if (widen_west)
            side = -offset;
        else if (widen_east)
            side = offset;
        else
            side = one_in_(2) ? -offset : offset;

        int y = y_lo + 2 + rand_int(3);
        while (y < y_hi - 1)
        {
            int nx = x + side;
            if (in_bounds_fully(y, nx) && cave_feat[y][nx] == FEAT_WALL_EXTRA
                && !(cave_info[y][nx] & (CAVE_ROOM | CAVE_ICKY)))
            {
                cave_set_feat(y, nx, FEAT_FLOOR);
                cave_corridor1[y][nx] = r1;
                cave_corridor2[y][nx] = r2;
            }
            y += 3 + rand_int(3);
            side = -side; /* alternate sides */
        }
    }

    /* Pillar lines break up wide halls without blocking flow */
    if (profile->treatment == TUNNEL_TREAT_PILLARS && profile->width >= 3)
    {
        int y = y_lo + 2 + rand_int(2);
        while (y <= y_hi - 2)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
            {
                cave_set_feat(y, x, FEAT_WALL_EXTRA);
                cave_corridor1[y][x] = -1;
                cave_corridor2[y][x] = -1;
            }
            y += 3 + rand_int(2);
        }
    }
}

static void apply_h_tunnel_treatment(
    int r1, int r2, int x_lo, int x_hi, int y, bool widen_north, bool widen_south,
    const tunnel_profile* profile)
{
    if (!profile)
        return;

    if (profile->treatment == TUNNEL_TREAT_NICHES)
    {
        int offset = (profile->width >= 3) ? 2 : 1;
        int side = 0;
        if (widen_north && widen_south)
            side = one_in_(2) ? -offset : offset;
        else if (widen_north)
            side = -offset;
        else if (widen_south)
            side = offset;
        else
            side = one_in_(2) ? -offset : offset;

        int x = x_lo + 2 + rand_int(3);
        while (x < x_hi - 1)
        {
            int ny = y + side;
            if (in_bounds_fully(ny, x) && cave_feat[ny][x] == FEAT_WALL_EXTRA
                && !(cave_info[ny][x] & (CAVE_ROOM | CAVE_ICKY)))
            {
                cave_set_feat(ny, x, FEAT_FLOOR);
                cave_corridor1[ny][x] = r1;
                cave_corridor2[ny][x] = r2;
            }
            x += 3 + rand_int(3);
            side = -side;
        }
    }

    if (profile->treatment == TUNNEL_TREAT_PILLARS && profile->width >= 3)
    {
        int x = x_lo + 2 + rand_int(2);
        while (x <= x_hi - 2)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
            {
                cave_set_feat(y, x, FEAT_WALL_EXTRA);
                cave_corridor1[y][x] = -1;
                cave_corridor2[y][x] = -1;
            }
            x += 3 + rand_int(2);
        }
    }
}

static void build_v_tunnel(
    int r1, int r2, int y1, int y2, int x, const tunnel_profile* profile)
{
    int y, y_lo, y_hi;
    tunnel_profile local = profile ? *profile : TUNNEL_PROFILE_NORMAL;
    int width = MAX(1, MIN(local.width, 3));
    bool short_span = (ABS(y2 - y1) < 4);
    if (short_span)
        local.treatment = TUNNEL_TREAT_NONE;
    if (short_span && width > 2)
        width = 2;

    bool widen_west = (width >= 3) || (width == 2 && local.side_bias < 0);
    bool widen_east = (width >= 3) || (width == 2 && local.side_bias > 0);

    y_lo = MIN(y1, y2);
    y_hi = MAX(y1, y2);

    for (y = y_lo; y <= y_hi; y++)
    {
        if (cave_feat[y][x] == FEAT_WALL_OUTER)
        {
            /* all doors get randomised later */
            cave_set_feat(y, x, FEAT_DOOR_HEAD);
        }
        else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
        }

        /* thicken corridors when requested by carving adjacent granite only */
        if (width > 1)
        {
            if (widen_east && x + 1 < MAX_DUNGEON_WID
                && cave_feat[y][x + 1] == FEAT_WALL_EXTRA
                && in_bounds_fully(y, x + 1)
                && !(cave_info[y][x + 1] & (CAVE_ROOM)))
            {
                cave_set_feat(y, x + 1, FEAT_FLOOR);
                cave_corridor1[y][x + 1] = r1;
                cave_corridor2[y][x + 1] = r2;
            }
            if (widen_west && x - 1 > 0 && cave_feat[y][x - 1] == FEAT_WALL_EXTRA
                && in_bounds_fully(y, x - 1)
                && !(cave_info[y][x - 1] & (CAVE_ROOM)))
            {
                cave_set_feat(y, x - 1, FEAT_FLOOR);
                cave_corridor1[y][x - 1] = r1;
                cave_corridor2[y][x - 1] = r2;
            }
        }
    }

    apply_v_tunnel_treatment(r1, r2, y_lo, y_hi, x, widen_west, widen_east,
        &local);
}

static void build_h_tunnel(
    int r1, int r2, int x1, int x2, int y, const tunnel_profile* profile)
{
    int x, x_lo, x_hi;
    tunnel_profile local = profile ? *profile : TUNNEL_PROFILE_NORMAL;
    int width = MAX(1, MIN(local.width, 3));
    bool short_span = (ABS(x2 - x1) < 4);
    if (short_span)
        local.treatment = TUNNEL_TREAT_NONE;
    if (short_span && width > 2)
        width = 2;

    bool widen_south = (width >= 3) || (width == 2 && local.side_bias > 0);
    bool widen_north = (width >= 3) || (width == 2 && local.side_bias < 0);

    x_lo = MIN(x1, x2);
    x_hi = MAX(x1, x2);

    for (x = x_lo; x <= x_hi; x++)
    {
        if (cave_feat[y][x] == FEAT_WALL_OUTER)
        {
            /* all doors get randomised later */
            cave_set_feat(y, x, FEAT_DOOR_HEAD);
        }
        else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
        }

        /* thicken corridors when requested by carving adjacent granite only */
        if (width > 1)
        {
            if (widen_south && y + 1 < MAX_DUNGEON_HGT
                && cave_feat[y + 1][x] == FEAT_WALL_EXTRA
                && in_bounds_fully(y + 1, x)
                && !(cave_info[y + 1][x] & (CAVE_ROOM)))
            {
                cave_set_feat(y + 1, x, FEAT_FLOOR);
                cave_corridor1[y + 1][x] = r1;
                cave_corridor2[y + 1][x] = r2;
            }
            if (widen_north && y - 1 > 0 && cave_feat[y - 1][x] == FEAT_WALL_EXTRA
                && in_bounds_fully(y - 1, x)
                && !(cave_info[y - 1][x] & (CAVE_ROOM)))
            {
                cave_set_feat(y - 1, x, FEAT_FLOOR);
                cave_corridor1[y - 1][x] = r1;
                cave_corridor2[y - 1][x] = r2;
            }
        }
    }

    apply_h_tunnel_treatment(r1, r2, x_lo, x_hi, y, widen_north, widen_south,
        &local);
}

static bool build_tunnel(
    int r1, int r2, int y1, int x1, int y2, int x2, bool tentative)
{
    tunnel_profile profile = choose_tunnel_profile(tentative);

    /* build a vertical tunnel */
    if (x1 == x2)
    {
        if (!v_tunnel_ok(y1, y2, x1, tentative, 2))
        {
            return (false);
        }
        build_v_tunnel(r1, r2, y1, y2, x1, &profile);
    }

    /* build a horizontal tunnel */
    else if (y1 == y2)
    {
        if (!h_tunnel_ok(x1, x2, y1, tentative, 2))
        {
            return (false);
        }
        build_h_tunnel(r1, r2, x1, x2, y1, &profile);
    }

    /* build an L-shaped tunnel */
    else
    {
        /* build an h-v tunnel */
        if (one_in_(2))
        {
            if (!h_tunnel_ok(x1, x2, y1, tentative, 1)
                || !v_tunnel_ok(y1, y2, x2, tentative, 1))
            {
                return (false);
            }
            build_h_tunnel(r1, r2, x1, x2, y1, &profile);
            build_v_tunnel(r1, r2, y1, y2, x2, &profile);
        }

        /* build a v-h tunnel */
        else
        {
            if (!h_tunnel_ok(x1, x2, y2, tentative, 1)
                || !v_tunnel_ok(y1, y2, x1, tentative, 1))
            {
                return (false);
            }
            build_v_tunnel(r1, r2, y1, y2, x1, &profile);
            build_h_tunnel(r1, r2, x1, x2, y2, &profile);
        }
    }

    return (true);
}

static bool connect_two_rooms(int r1, int r2, bool tentative, bool desperate)
{
    int x, y;
    int r1y, r1x, r1y1, r1x1, r1y2, r1x2;
    int r2y, r2x, r2y1, r2x1, r2y2, r2x2;
    bool success;

    int distance_limitx = desperate ? 22 : 15;
    int distance_limity = desperate ? 16 : 10;

    r1y = dun->cent[r1].y;
    r1x = dun->cent[r1].x;
    r1y1 = dun->corner[r1].y1;
    r1x1 = dun->corner[r1].x1;
    r1y2 = dun->corner[r1].y2;
    r1x2 = dun->corner[r1].x2;

    r2y = dun->cent[r2].y;
    r2x = dun->cent[r2].x;
    r2y1 = dun->corner[r2].y1;
    r2x1 = dun->corner[r2].x1;
    r2y2 = dun->corner[r2].y2;
    r2x2 = dun->corner[r2].x2;

    /* if the rooms are too far apart, then just give up immediately */
    // look at total distance of room centres
    if ((ABS(r1y - r2y) > distance_limity * 3)
        || (ABS(r1x - r2x) > distance_limitx * 3))
    {
        return (false);
    }
    // then look at distance of relevant room edges
    if ((r1x < r2x) && (r2x1 - r1x2 > distance_limitx))
    {
        return (false);
    }
    if ((r2x < r1x) && (r1x1 - r2x2 > distance_limitx))
    {
        return (false);
    }
    if ((r1y < r2y) && (r2y1 - r1y2 > distance_limity))
    {
        return (false);
    }
    if ((r2y < r1y) && (r1y1 - r2y2 > distance_limity))
    {
        return (false);
    }

    /* if we have vertical or horizontal overlap, connect a straight tunnel */
    /* at a random point where they overlap */

    /* if vertical overlap */
    if ((r1x1 <= r2x2) && (r2x1 <= r1x2))
    {
        /* unless careful, there will be too many vertical tunnels */
        /* since rooms are wider than they are tall                */
        if (tentative && one_in_(2))
        {
            return (false);
        }
        x = rand_range(MAX(r1x1, r2x1),
            MIN(r1x2,
                r2x2)); // Sil-x: one of these two lines has somehow caused a
                        // crash:
                        // http://angband.oook.cz/ladder-show.php?id=13070

        success = build_tunnel(r1, r2, r1y, x, r2y, x, tentative);
    }
    /* if horizontal overlap */
    else if ((r1y1 <= r2y2) && (r2y1 <= r1y2))
    {
        y = rand_range(MAX(r1y1, r2y1),
            MIN(r1y2,
                r2y2)); // Sil-x: one of these two lines has somehow caused a
                        // crash

        success = build_tunnel(r1, r2, y, r1x, y, r2x, tentative);
    }

    /* otherwise, make an L shaped corridor between their centres */
    else
    {
        // this must fail if any of the tunnels would be too long
        if (MIN(ABS(r2x - r1x1), ABS(r2x - r1x2)) > distance_limitx - 2)
            return (false);
        if (MIN(ABS(r1x - r2x1), ABS(r1x - r2x2)) > distance_limitx - 2)
            return (false);
        if (MIN(ABS(r2y - r1y1), ABS(r2y - r1y2)) > distance_limity - 2)
            return (false);
        if (MIN(ABS(r1y - r2y1), ABS(r1y - r2y2)) > distance_limity - 2)
            return (false);

        success = build_tunnel(r1, r2, r1y, r1x, r2y, r2x, tentative);
    }

    if (success)
    {
        dun->connection[r1][r2] = true;
        dun->connection[r2][r1] = true;
    }

    return (success);
}

static bool connect_room_to_corridor(int r)
{
    int length = 10;
    int x;
    int y;
    int delta;
    int ry, rx, r1, r2;
    bool success = false;
    bool done = false;

    ry = dun->cent[r].y;
    rx = dun->cent[r].x;

    y = ry;
    x = rx;

    // go down/right half the time, up/left the other half
    if (one_in_(2))
        delta = 1;
    else
        delta = -1;

    // go horizontal half the time, vertical the other half
    if (one_in_(2))
    {
        while (!done)
        {
            y += delta;

            // abort if the tunnel leaves the map or passes through a door
            if (!in_bounds(y, x) || (ABS(y - ry) > length)
                || cave_any_closed_door_bold(y, x))
            {
                success = false;
                done = true;
            }

            // it has intercepted a tunnel!
            else if ((cave_feat[y][x] == FEAT_FLOOR)
                && !(cave_info[y][x] & (CAVE_ROOM)))
            {
                r1 = cave_corridor1[y][x];
                r2 = cave_corridor2[y][x];

                // make sure that the tunnel intercepts only connects rooms that
                // aren't connected to this room
                if ((r1 < 0) || (r2 < 0)
                    || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
                {
                    if (v_tunnel_ok(ry, y - (delta * 2), x, true, 1))
                    {
                        build_v_tunnel(r, r1, ry, y, x, &TUNNEL_PROFILE_NORMAL);

                        // mark the new room connections
                        dun->connection[r][r1] = true;
                        dun->connection[r1][r] = true;
                        dun->connection[r][r2] = true;
                        dun->connection[r2][r] = true;
                        success = true;
                    }
                }

                done = true;
            }
        }
    }

    // do the vertical case (very similar to the horizontal one!)
    else
    {
        while (!done)
        {
            x += delta;

            // abort if the tunnel leaves the map or passes through a door
            if (!in_bounds(y, x) || (ABS(x - rx) > length)
                || cave_any_closed_door_bold(y, x))
            {
                success = false;
                done = true;
            }

            // it has intercepted a tunnel!
            else if ((cave_feat[y][x] == FEAT_FLOOR)
                && !(cave_info[y][x] & (CAVE_ROOM)))
            {
                r1 = cave_corridor1[y][x];
                r2 = cave_corridor2[y][x];

                // make sure that the tunnel intercepts only connects rooms that
                // aren't connected to this room
                if ((r1 < 0) || (r2 < 0)
                    || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
                {
                    if (h_tunnel_ok(rx, x - (delta * 2), y, true, 1))
                    {
                        build_h_tunnel(r, r1, rx, x, y, &TUNNEL_PROFILE_NORMAL);

                        // mark the new room connections
                        dun->connection[r][r1] = true;
                        dun->connection[r1][r] = true;
                        dun->connection[r][r2] = true;
                        dun->connection[r2][r] = true;
                        success = true;
                    }
                }

                done = true;
            }
        }
    }

    return (success);
}

/*
 * Places some staircases near walls
 */
static bool alloc_stairs(int feat, int num)
{
    int x;

    /*smaller levels don't need that many stairs, but there are a minimum of 4
     * rooms*/
    if (dun->cent_n == 4)
        num = 1;
    else if (num > (dun->cent_n / 2))
        num = dun->cent_n / 2;

    /* Place "num" stairs */
    for (x = 0; x < num; x++)
    {
        int i;

        int yy, xx;

        for (i = 0; i <= 1000; i++)
        {
            yy = rand_int(p_ptr->cur_map_hgt);
            xx = rand_int(p_ptr->cur_map_wid);

            /* make sure the square is empty, in a room and has no adjacent
             * doors*/
            if (cave_naked_bold(yy, xx) && (cave_info[yy][xx] & (CAVE_ROOM)))
                if ((cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD))
                {
                    break;
                }
            if (i == 1000)
            {
                return (false);
            }
        }

        /* Surface -- must go down */
        if (!p_ptr->depth)
        {
            /* Clear previous contents, add down stairs */
            cave_set_feat(yy, xx, FEAT_MORE);
        }

        /* Bottom -- must go up */
        else if (p_ptr->depth >= MORGOTH_DEPTH)
        {
            /* Clear previous contents, add up stairs */
            if (x != 0)
                cave_set_feat(yy, xx, FEAT_LESS);
            else
                cave_set_feat(yy, xx, choose_up_stairs());
        }

        /* Requested type */
        else
        {
            /* Allow shafts, but guarantee the first one is an ordinary stair */
            if (x != 0)
            {
                if (feat == FEAT_LESS)
                    feat = choose_up_stairs();
                else if (feat == FEAT_MORE)
                    feat = choose_down_stairs();
            }

            /* Clear previous contents, add stairs */
            cave_set_feat(yy, xx, feat);
        }
    }

    return (true);
}

bool feat_within_los(int y0, int x0, int feat)
{
    int y, x;

    bool detect = false;

    /* Scan the visible area */
    for (y = y0 - 15; y < y0 + 15; y++)
    {
        for (x = x0 - 15; x < x0 + 15; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!los(y0, x0, y, x))
                continue;

            /* Detect invisible traps */
            if (cave_feat[y][x] == feat)
            {
                detect = true;
            }
        }
    }

    /* Result */
    return (detect);
}

/*
 * Are there any stairs within line of sight?
 */
bool stairs_within_los(int y, int x)
{
    if (feat_within_los(y, x, FEAT_LESS))
        return (true);
    if (feat_within_los(y, x, FEAT_MORE))
        return (true);
    if (feat_within_los(y, x, FEAT_LESS_SHAFT))
        return (true);
    if (feat_within_los(y, x, FEAT_MORE_SHAFT))
        return (true);

    // else:

    return (false);
}

/*
 * Determines the chance (out of 1000) that a trap will be placed in a given
 * square.
 */
static int trap_placement_chance(int y, int x)
{
    int yy, xx;

    int chance = 0;
    /* extra traps from CUR_TRAPS */
    int bonus_traps = curse_flag_count_cur(CUR_TRAPS);
    if (bonus_traps)
        chance += 10 * bonus_traps;   /* +10/20/30 ÔÇª on top of normal */

    // extra chance of having a trap for certain squares inside rooms
    if (cave_clean_bold(y, x) && (cave_info[y][x] & (CAVE_ROOM)))
    {
        chance = 1;

        // check the squares that neighbour (y,x)
        for (yy = y - 1; yy <= y + 1; yy++)
        {
            for (xx = x - 1; xx <= x + 1; xx++)
            {
                if (!((yy == y) && (xx == x)))
                {
                    // item
                    if (cave_o_idx[yy][xx] != 0)
                        chance += 10;

                    // stairs
                    if (cave_stair_bold(yy, xx))
                        chance += 10;

                    // closed doors (including secret)
                    if (cave_any_closed_door_bold(yy, xx))
                        chance += 10;
                }
            }
        }

        // opposing impassable squares (chasm or wall)
        if (cave_impassable_bold(y - 1, x) && cave_impassable_bold(y + 1, x))
            chance += 10;
        if (cave_impassable_bold(y, x - 1) && cave_impassable_bold(y, x + 1))
            chance += 10;
    }

    return (chance);
}

/*
 * Place traps randomly on the level.
 * Biased towards certain sneaky locations.
 */
static void place_traps(void)
{
    int y, x;

    // scan the map
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            // randomly determine whether to place a trap based on the above
            if (dieroll(1000) <= trap_placement_chance(y, x))
            {
                place_trap(y, x);
            }
        }
    }
}

static bool place_rubble_player(void)
{
    int r;
    int y, x;
    int i, panels;

    /* Basic "amount" */

    panels = (p_ptr->cur_map_hgt / PANEL_HGT)
        * (p_ptr->cur_map_wid / PANEL_WID_FIXED);

    r = dieroll(panels / 3);

    // occasionally produce much more rubble on deep levels
    if ((p_ptr->depth >= 10) && one_in_(10))
        r += panels * 2;

    /* Put some rubble in corridors */
    alloc_object(ALLOC_SET_BOTH, ALLOC_TYP_RUBBLE, r, false);

    /* simple way to place player */
    for (i = 0; i <= 100; i++)
    {
        y = rand_int(p_ptr->cur_map_hgt);
        x = rand_int(p_ptr->cur_map_wid);

        // require empty square that isn't in an interesting room or vault
        if (cave_naked_bold(y, x) && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            // require a room if it is the first level
            if ((playerturn > 0) || (cave_info[y][x] & (CAVE_ROOM)))
            {
                // don't generate stairs within line of sight if player arrived
                // using stairs
                if (!stairs_within_los(y, x) || (p_ptr->create_stair == false))
                {
                    player_place(y, x);
                    break;
                }
            }
        }
        if (i == 100)
        {
            log_trace("place_rubble_player failed: Could not find suitable player placement after 100 attempts");
            return (false);
        }
    }

    return (true);
}

/*
 *  Make sure that the level is sufficiently connected.
 */

bool check_connectivity(void)
{
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    int y, x;

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    // Make sure entire dungeon is connected (ignoring rubble and chasms)
    flood_access(p_ptr->py, p_ptr->px, cave_access, true);
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            if (player_passable(y, x, true) && (cave_access[y][x] == false))
            {
                return (false);
            }

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    if (p_ptr->create_stair == FEAT_MORE
        || p_ptr->create_stair == FEAT_MORE_SHAFT)
    {
        return (true);
    }

    // Make sure player can reach down stairs without going through rubble and
    // chasms
    flood_access(p_ptr->py, p_ptr->px, cave_access, false);
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (((cave_feat[y][x] == FEAT_MORE) && (cave_access[y][x] == true))
                || ((cave_feat[y][x] == FEAT_MORE_SHAFT)
                    && (cave_access[y][x] == true)))
            {
                return (true);
            }
        }

    return (false);
}

/*
 *  Check if there are two adjacent doors on the level.
 */
bool doubled_doors(void)
{
    int y, x;

    // Check each grid within boundary
    for (y = 0; y < p_ptr->cur_map_hgt - 1; y++)
        for (x = 0; x < p_ptr->cur_map_wid - 1; x++)
            if (cave_known_closed_door_bold(y, x))
            {
                if (cave_known_closed_door_bold(y + 1, x))
                    return (true);
                if (cave_known_closed_door_bold(y, x + 1))
                    return (true);
            }

    return (false);
}

static bool connect_rooms_stairs(void)
{
    int i;
    int corridor_attempts;
    int r1, r2, r_closest, d_closest, d;
    int pieces = 0;

    int width;
    int stairs = 0;
    int initial_up = FEAT_LESS;
    int initial_down = FEAT_MORE;

    bool joined;
    bool anchor_linked_any = false;

    /* Pre-pass: ensure neighbor-required anchors get at least one connection */
    for (int a = 0; a < layout_anchor_count; ++a)
    {
        layout_anchor_t *anchor = &layout_anchors[a];
        if (anchor->room_slot < 0 || anchor->room_slot >= dun->cent_n)
            continue;
        if (!anchor->requires_neighbor)
            continue;

        int best_idx = -1;
        int best_dist = 9999;
        for (int b = 0; b < layout_anchor_count; ++b)
        {
            if (a == b)
                continue;
            layout_anchor_t *other = &layout_anchors[b];
            if (other->room_slot < 0 || other->room_slot >= dun->cent_n)
                continue;
            int dist = distance(anchor->center.y, anchor->center.x, other->center.y, other->center.x);
            if (dist < best_dist)
            {
                best_dist = dist;
                best_idx = other->room_slot;
            }
        }

        if (best_idx >= 0 && !dun->connection[anchor->room_slot][best_idx])
        {
            if (connect_two_rooms(anchor->room_slot, best_idx, true, true))
            {
                anchor->neighbor_linked = true;
                anchor_linked_any = true;
            }
        }
    }

    if (anchor_linked_any)
    {
        log_trace("Anchor pre-pass: connected neighbor-required anchors where needed");
    }

    // Phase 1:
    // connect each room to the closest room (if not already connected)

    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        /* find closest room */
        r_closest = 0; /* default values that will get beaten trivially */
        d_closest = 1000;
        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 != r1)
            {
                d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y,
                    dun->cent[r2].x);
                if (d < d_closest)
                {
                    d_closest = d;
                    r_closest = r2;
                }
            }
        }

        /* connect the rooms, if not already connected */
        if (!(dun->connection[r1][r_closest]))
        {
            (void)connect_two_rooms(r1, r_closest, true, false);
        }
    }

    // Phase 2:
    // make some random connections between rooms so long as they don't
    // intersect things

    switch (p_ptr->cur_map_hgt / PANEL_HGT)
    {
    case 3:
        corridor_attempts = dun->cent_n * dun->cent_n;
    case 4:
        corridor_attempts = dun->cent_n * dun->cent_n * 2;
    case 5:
        corridor_attempts = dun->cent_n * dun->cent_n * 10;
    default:
        corridor_attempts = dun->cent_n * dun->cent_n * 10;
    }

    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        r2 = rand_int(dun->cent_n);
        if ((r1 != r2) && !(dun->connection[r1][r2]))
        {
            (void)connect_two_rooms(r1, r2, true, false);
        }
    }

    // add some T-intersections in the corridors
    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        (void)connect_room_to_corridor(r1);
    }

    // Phase 3:
    // cut the dungeon up into connected pieces and try hard to make corridors
    // that connect them

    pieces = dungeon_pieces();
    while (pieces > 1)
    {
        joined = false;

        for (r1 = 0; r1 < dun->cent_n; r1++)
        {
            for (r2 = 0; r2 < dun->cent_n; r2++)
            {
                if (!joined && (dun->piece[r1] != dun->piece[r2]))
                {
                    for (i = 0; i < 10; i++)
                    {
                        if (!(dun->connection[r1][r2]))
                        {
                            joined = connect_two_rooms(r1, r2, true, true);
                        }
                    }
                }
            }
        }

        if (!joined)
            break;

        // cut the dungeon up into connected pieces and stop if there is only
        // one
        pieces = dungeon_pieces();
    }

    // label_rooms();

    /* Place down stairs */
    width = (p_ptr->cur_map_wid / PANEL_WID_FIXED);

    if (width <= 3)
        stairs = 1;
    else if (width == 4)
        stairs = 2;
    else
        stairs = 4;

    if (p_ptr->on_the_run)
    {
        initial_down = FEAT_MORE_SHAFT;
        stairs *= 2;
    }

    if ((p_ptr->create_stair == FEAT_MORE)
        || (p_ptr->create_stair == FEAT_MORE_SHAFT))
        stairs--;
    
    if (!(alloc_stairs(initial_down, stairs)))
    {
        if (cheat_room)
            msg_format("Failed to place down stairs.");
        log_trace("connect_rooms_stairs failed: Could not place %d down stairs", stairs);
        return (false);
    }

    /* Place up stairs */

    if (width <= 3)
        stairs = 1;
    else if (width == 4)
        stairs = 2;
    else
        stairs = 4;

    if (p_ptr->on_the_run && p_ptr->depth >= 2)
    {
        initial_up = FEAT_LESS_SHAFT;
        stairs *= 2;
    }

    if ((p_ptr->create_stair == FEAT_LESS)
        || (p_ptr->create_stair == FEAT_LESS_SHAFT))
        stairs--;
    
    if (!(alloc_stairs(initial_up, stairs)))
    {
        if (cheat_room)
            msg_format("Failed to place up stairs.");
        log_trace("connect_rooms_stairs failed: Could not place %d up stairs", stairs);
        return (false);
    }

    /* Hack -- Add some quartz streamers */
    for (i = 0; i < DUN_STR_QUA; i++)
    {
        /*if we can't build streamers, something is wrong with level*/
        if (!build_streamer(FEAT_QUARTZ))
        {
            log_trace("connect_rooms_stairs failed: Could not build quartz streamer %d", i);
            return (false);
        }
    }

    // add any chasms if needed
    build_chasms();

    return (true);
}

/*
 * Room building routines.
 *
 * Six basic room types:
 *   1 -- normal
 *   2 -- cross shaped
 *   3 -- (removed)
 *   4 -- large room with features (removed)
 *   5 -- monster nests (removed)
 *   6 -- least vaults (formerly: monster pits)
 *   7 -- lesser vaults
 *   8 -- greater vaults
 */

/*
 * Forward declaration for quest vault helper
 */
static bool solid_rock_reduced_padding(int y1, int x1, int y2, int x2);
static bool place_room_forced(int y0, int x0, vault_type* v_ptr);
static bool try_quest_vault_type(int vault_type);

/*
 * Type 1 -- normal rectangular rooms
 */
static bool build_type1(int y0, int x0)
{
    int y, x;

    int y1, x1, y2, x2;

    int light = false;

    // Occasional light - chance of darkness starts very small and
    // increases quadratically until always dark at 950 ft
    if ((p_ptr->depth < dieroll(MORGOTH_DEPTH - 1))
        || (p_ptr->depth < dieroll(MORGOTH_DEPTH - 1)))
    {
        light = true;
    }

    /* Pick a room size */
    y1 = y0 - dieroll(3);
    x1 = x0 - dieroll(5);
    y2 = y0 + dieroll(3);
    x2 = x0 + dieroll(4) + 1;

    /* Sil: bounds checking */
    if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
        || (x2 >= p_ptr->cur_map_wid - 3))
    {
        return (false);
    }

    /* Check to see if the location is all plain rock */
    if (!solid_rock(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
    {
        return (false);
    }

    if (doubled_wall(y1, x1, y2, x2))
    {
        return (false);
    }

    /* Save the corner locations */
    dun->corner[dun->cent_n].y1 = y1;
    dun->corner[dun->cent_n].x1 = x1;
    dun->corner[dun->cent_n].y2 = y2;
    dun->corner[dun->cent_n].x2 = x2;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = ROOM_KIND_CLASSIC;
    dun->is_quest[dun->cent_n] = false;
    dun->cent_n++;

    /* Generate new room */
    generate_room(y1 - 1, x1 - 1, y2 + 1, x2 + 1, light);

    /* Generate outer walls */
    generate_draw(y1 - 1, x1 - 1, y2 + 1, x2 + 1, FEAT_WALL_OUTER);

    /* Generate inner floors */
    generate_fill(y1, x1, y2, x2, FEAT_FLOOR);

    /* Hack -- Occasional pillar room */
    if (one_in_(20) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
    {
        for (y = y1 + 1; y <= y2; y += 2)
        {
            for (x = x1 + 1; x <= x2; x += 2)
            {
                cave_set_feat(y, x, FEAT_WALL_INNER);
            }
        }
    }

    /* Hack -- Occasional pillar-lined room */
    if (one_in_(10) && ((x2 - x1) % 2 == 0) && ((y2 - y1) % 2 == 0))
    {
        for (y = y1 + 1; y <= y2; y += 2)
        {
            for (x = x1 + 1; x <= x2; x += 2)
            {
                if ((x == x1 + 1) || (x == x2 - 1) || (y == y1 + 1)
                    || (y == y2 - 1))
                {
                    cave_set_feat(y, x, FEAT_WALL_INNER);
                }
            }
        }
    }

    return (true);
}

/*
 * Type 2 -- Cross shaped rooms
 */
static bool build_type2(int y0, int x0)
{
    int y, x;

    int y1h, x1h, y2h, x2h;
    int y1v, x1v, y2v, x2v;

    int h_hgt, h_wid, v_hgt, v_wid;

    int light = false;

    /* Occasional light - always at level 1 through to never at Morgoth's level
     */
    if (p_ptr->depth < dieroll(MORGOTH_DEPTH))
        light = true;

    /* Pick a room size */

    h_hgt = 1; /* 3 */
    h_wid = rand_range(5, 7); /* 11, 13, 15 */

    y1h = y0 - h_hgt;
    x1h = x0 - h_wid;
    y2h = y0 + h_hgt;
    x2h = x0 + h_wid;

    v_hgt = rand_range(3, 6); /* 7, 9, 11, 13 */
    v_wid = rand_range(1, 2); /* 3, 5 */

    y1v = y0 - v_hgt;
    x1v = x0 - v_wid;
    y2v = y0 + v_hgt;
    x2v = x0 + v_wid;

    /* Sil: bounds checking */
    if ((y1v <= 3) || (x1h <= 3) || (y2v >= p_ptr->cur_map_hgt - 3)
        || (x2h >= p_ptr->cur_map_wid - 3))
    {
        return (false);
    }

    /* Check to see if the location is all plain rock */
    if (!solid_rock(y1v - 1, x1h - 1, y2v + 1, x2h + 1))
    {
        return (false);
    }

    if (doubled_wall(y1v, x1h, y2v, x2h))
    {
        return (false);
    }

    /* Save the corner locations */
    dun->corner[dun->cent_n].y1 = y1v;
    dun->corner[dun->cent_n].x1 = x1h;
    dun->corner[dun->cent_n].y2 = y2v;
    dun->corner[dun->cent_n].x2 = x2h;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = ROOM_KIND_CROSS;
    dun->is_quest[dun->cent_n] = false;
    dun->cent_n++;

    /* Generate new room */
    generate_room(y1h - 1, x1h - 1, y2h + 1, x2h + 1, light);
    generate_room(y1v - 1, x1v - 1, y2v + 1, x2v + 1, light);

    /* Generate outer walls */
    generate_draw(y1h - 1, x1h - 1, y2h + 1, x2h + 1, FEAT_WALL_OUTER);
    generate_draw(y1v - 1, x1v - 1, y2v + 1, x2v + 1, FEAT_WALL_OUTER);

    /* Generate inner floors */
    generate_fill(y1h, x1h, y2h, x2h, FEAT_FLOOR);
    generate_fill(y1v, x1v, y2v, x2v, FEAT_FLOOR);

    /* Hack -- Occasional pillar room */

    switch (dieroll(7))
    {
    case 1:
    {
        if ((v_wid == 2) && (v_hgt == 6))
        {
            for (y = y1v + 1; y <= y2v; y += 2)
            {
                for (x = x1v + 1; x <= x2v; x += 2)
                {
                    cave_set_feat(y, x, FEAT_WALL_INNER);
                }
            }
            place_object(y0, x0, false, false, DROP_TYPE_CHEST);
        }
        break;
    }
    case 2:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            generate_plus(y0 - 1, x0 - 1, y0 + 1, x0 + 1, FEAT_WALL_INNER);
        }
        break;
    }
    case 3:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            cave_set_feat(y0 - 1, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0 - 1, x0 + 1, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0 + 1, FEAT_WALL_INNER);
        }
        break;
    }
    case 4:
    {
        if ((v_wid == 1) && (h_hgt == 1))
        {
            cave_set_feat(y0, x0 - 1, FEAT_WALL_INNER);
            cave_set_feat(y0, x0 + 1, FEAT_WALL_INNER);
            cave_set_feat(y0 - 1, x0, FEAT_WALL_INNER);
            cave_set_feat(y0 + 1, x0, FEAT_WALL_INNER);
        }
        break;
    }
    default:
    {
        break;
    }
    }

    return (true);
}

/*
 *  Has a very good go at placing a monster of kind represented by a flag
 *  (eg RF3_DRAGON) at (y,x). It is goverened by a maximum depth and tries
 *  100 times at this depth and each depth below it.
 */
extern void place_monster_by_flag(
    int y, int x, int flagset, u32b f, bool allow_unique, int max_depth)
{
    bool got_r_idx = false;
    int tries = 0;
    int r_idx;
    monster_race* r_ptr;
    int depth = max_depth;

    while (!got_r_idx && (depth > 0))
    {
        r_idx = get_mon_num(depth, false, true, true);
        r_ptr = &r_info[r_idx];

        if (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE)))
        {
            if (((flagset == 1) && (r_ptr->flags1 & (f)))
                || ((flagset == 2) && (r_ptr->flags2 & (f)))
                || ((flagset == 3) && (r_ptr->flags3 & (f)))
                || ((flagset == 4) && (r_ptr->flags4 & (f))))
            {
                got_r_idx = true;
                break;
            }
        }

        tries++;
        if (tries >= 100)
        {
            tries = 0;
            depth--;
        }
    }

    // place a monster of that type if you could find one
    if (got_r_idx)
        place_monster_one(y, x, r_idx, true, false, NULL);
}

/*
 *  Has a very good go at placing a monster of kind represented by its racial
 * letter (eg 'v' for vampire) at (y,x). It is goverened by a maximum depth and
 * tries 100 times at this depth and each depth below it.
 */
void place_monster_by_letter(
    int y, int x, char c, bool allow_unique, int max_depth)
{
    bool got_r_idx = false;
    int tries = 0;
    int r_idx;
    monster_race* r_ptr;
    int depth = max_depth;

    while (!got_r_idx && (depth > 0))
    {
        r_idx = get_mon_num(depth, false, true, true);
        r_ptr = &r_info[r_idx];
        if ((r_ptr->d_char = c)
            && (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE))))
        {
            got_r_idx = true;
            break;
        }

        tries++;
        if (tries >= 100)
        {
            tries = 0;
            depth--;
        }
    }

    // place a monster of that type if you could find one
    if (got_r_idx)
        place_monster_one(y, x, r_idx, true, false, NULL);
}

/*
 * Hack -- fill in "vault" rooms
 */
static bool build_vault(int y0, int x0, vault_type* v_ptr, bool flip_d)
{
    int ymax = v_ptr->hgt;
    int xmax = v_ptr->wid;
    cptr data = v_text + v_ptr->text;
    int dx, dy, x, y;
    int ax, ay;
    bool flip_v = false;
    bool flip_h = false;
    int multiplier;

    int original_object_level = object_level;
    int original_monster_level = monster_level;

    log_trace("build_vault: Building vault '%s' with color=%d at center (%d,%d), size %dx%d", 
              v_name + v_ptr->name, v_ptr->color, y0, x0, xmax, ymax);
    log_trace("build_vault: Vault flags = 0x%x, flip_d = %s", v_ptr->flags, flip_d ? "true" : "false");
    
    /* DEBUGGING: Check if this is a quest vault */
    if (v_ptr->flags & VLT_QUEST) {
        log_trace("build_vault: *** QUEST VAULT DETECTED *** Building '%s'", v_name + v_ptr->name);
    }

    cptr t;

    // Check that the vault doesn't contain invalid things for its depth
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        for (dx = 0; dx < xmax; dx++, t++)
        {
            // Barrow wights can't be deeper than level 13
            if ((*t == 'W') && (p_ptr->depth > 13))
            {
                log_debug("Skipped a barrow wight vault.");
                return (false);
            }

            // chasms can't occur at 950 ft
            if ((*t == '7') && (p_ptr->depth >= MORGOTH_DEPTH - 1))
            {
                return (false);
            }
        }
    }

    // reflections
    if ((p_ptr->depth > 0) && (p_ptr->depth < MORGOTH_DEPTH))
    {
        // reflect it vertically half the time
        if (one_in_(2))
            flip_v = true;

        // reflect it horizontally half the time
        if (one_in_(2))
            flip_h = true;
    }

    /* Begin the vault style context now that the vault is accepted */
    styles_begin_vault(-1, 0);
    /* If vault has explicit style list, use it (support '*'=-1); else apply per-depth default */
    styles_reset_vault_weights();
    if (v_ptr->style_count > 0) {
        for (int si = 0; si < v_ptr->style_count; ++si) {
            int sidx = v_ptr->style_idx[si];
            int w = v_ptr->style_weight[si];
            if (sidx == -1) {
                int lp = styles_get_level_primary_style();
                if (lp >= 0) styles_add_vault_weight(lp, w);
            } else if (sidx == -2) {
                /* '$' token: pick one random style from the current level's
                 * available list and add it with the specified weight. */
                int rs = styles_pick_random_from_level();
                if (rs >= 0) styles_add_vault_weight(rs, w);
            } else {
                styles_add_vault_weight(sidx, w);
            }
        }
    } else {
        /* No S: provided ÔÇö choose a random style from the depth-available list */
        int rs = styles_pick_random_from_level();
        if (rs >= 0) styles_add_vault_weight(rs, 1);
    }
    /* Choose one primary style for the entire vault */
    styles_select_vault_primary();
    log_debug("build_vault: level_primary=%d vault_primary=%d",
        styles_get_level_primary_style(), styles_get_vault_primary_style());

    /* Place dungeon features and objects */
    int vault_primary_sidx_for_encoding = styles_get_vault_primary_style();
    int v_min_y = 32767, v_min_x = 32767, v_max_y = -32768, v_max_x = -32768; /* track vault bbox */
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the location */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" but still advance bbox only on placed tiles */
            if (*t == ' ')
                continue;

            /* Track bbox of actual vault content */
            if (y < v_min_y) v_min_y = y;
            if (y > v_max_y) v_max_y = y;
            if (x < v_min_x) v_min_x = x;
            if (x > v_max_x) v_max_x = x;

            /* Lay down a floor, encoding the vault style and forcing first variant */
            if (vault_primary_sidx_for_encoding >= 0) {
                int enc = COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR + (vault_primary_sidx_for_encoding & (COLOR_STYLE_SLOT_MAX - 1));
                cave_set_feat_with_color(y, x, FEAT_FLOOR, enc);
            } else {
                cave_set_feat(y, x, FEAT_FLOOR);
            }

            /* Part of a vault */
            cave_info[y][x] |= (CAVE_ROOM | CAVE_ICKY);

            /* Analyze the grid */
            switch (*t)
            {
            /* Granite wall (outer) */
            case '$':
            {
                cave_set_feat_with_color(y, x, FEAT_WALL_OUTER, 0);
                break;
            }
            /* Granite wall (inner) */
            case '#':
            {
                cave_set_feat_with_color(y, x, FEAT_WALL_INNER, 0);
                break;
            }

            /* Quartz vein */
            case '%':
            {
                cave_set_feat_with_color(y, x, FEAT_QUARTZ, 0);
                break;
            }

            /* Rubble */
            case ':':
            {
                cave_set_feat_with_color(y, x, FEAT_RUBBLE, 0);
                break;
            }

            /* Glyph of warding */
            case ';':
            {
                cave_set_feat(y, x, FEAT_GLYPH);
                break;
            }

                /* Down staircase */
            case '>':
            {
                cave_set_feat(y, x, FEAT_MORE);
                break;
            }

            /* Up staircase */
            case '<':
            {
                cave_set_feat(y, x, FEAT_LESS);
                break;
            }

            /* Visible door */
            case '+':
            {
                place_closed_door(y, x);
                break;
            }

            /* Secret door */
            case 's':
            {
                place_secret_door(y, x);
                break;
            }

            /* Trap */
            case '^':
            {
                if (one_in_(2))
                    place_trap(y, x);
                break;
            }

            /* Forge */
            case '0':
            {
                place_forge(y, x);
                break;
            }

            /* Chasm */
            case '7':
            {
                cave_set_feat(y, x, FEAT_CHASM);
                break;
            }

            /* Sunlight */
            case ',':
            {
                cave_set_feat(y, x, FEAT_SUNLIGHT);
                break;
            }

            /* Not actually part of the vault after all */
            case ' ':
            {
                // remove room and vault flags
                cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);
                break;
            }
            }
        }
    }

    /* After placement, apply a 1-tile style halo so adjacent walls/floors match the vault style.
     * Refined: do NOT recolor corridor floor tiles that sit just outside a vault door.
     * We only halo floors when adjacent to a vault wall (not a door), to keep vault
     * entrances blending into the corridor style. Doors themselves remain excluded. */
    if (v_min_y <= v_max_y && v_min_x <= v_max_x) {
        int ay0 = MAX(1, v_min_y - 1);
        int ax0 = MAX(1, v_min_x - 1);
        int ay1 = MIN(p_ptr->cur_map_hgt - 2, v_max_y + 1);
        int ax1 = MIN(p_ptr->cur_map_wid - 2, v_max_x + 1);
        for (int yy = ay0; yy <= ay1; ++yy) {
            for (int xx = ax0; xx <= ax1; ++xx) {
                /* Skip squares that are already part of the vault */
                if (cave_info[yy][xx] & (CAVE_ICKY)) continue;

                /* Only halo cells adjacent to vault content (8-directional),
                 * and classify what kind of vault neighbor it is. */
                bool near_vault_any = false;
                bool near_vault_wall = false;
                bool near_vault_door = false;
                for (int dy2 = -1; dy2 <= 1; ++dy2) {
                    for (int dx2 = -1; dx2 <= 1; ++dx2) {
                        if (dy2 == 0 && dx2 == 0) continue;
                        int ny = yy + dy2, nx = xx + dx2;
                        if (!(cave_info[ny][nx] & (CAVE_ICKY))) continue;
                        near_vault_any = true;
                        int nfeat = cave_feat[ny][nx];
                        /* Door features */
                        if (nfeat == FEAT_OPEN || nfeat == FEAT_BROKEN ||
                            (nfeat >= FEAT_DOOR_HEAD && nfeat <= FEAT_DOOR_TAIL)) {
                            near_vault_door = true;
                        }
                        /* Walls and wall-like */
                        else if ((nfeat >= FEAT_WALL_HEAD && nfeat <= FEAT_WALL_TAIL) ||
                                 nfeat == FEAT_QUARTZ || nfeat == FEAT_RUBBLE) {
                            near_vault_wall = true;
                        }
                    }
                }
                if (!near_vault_any) continue;

                int feat = cave_feat[yy][xx];
                /* Skip doors; let corridor/door visuals remain level-styled */
                if (feat == FEAT_OPEN || feat == FEAT_BROKEN ||
                    (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)) {
                    continue;
                }

                /* Apply to floors only when adjacent to vault walls and NOT adjacent to vault doors */
                if (cave_floorlike_bold(yy, xx)) {
                    if (!(near_vault_wall && !near_vault_door)) continue;
                }
                /* Apply to walls/veins/rubble regardless, to blend the boundary */
                else if ((cave_info[yy][xx] & (CAVE_WALL)) || feat == FEAT_QUARTZ || feat == FEAT_RUBBLE) {
                    /* ok */
                } else {
                    continue;
                }

                {
                    /* Re-encode color to the vault primary style, forcing first variant */
                    int sidx = styles_get_vault_primary_style();
                    if (sidx < 0) sidx = styles_get_level_primary_style();
                    int enc = COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR + (sidx & (COLOR_STYLE_SLOT_MAX - 1));
                    cave_set_feat_with_color(yy, xx, feat, enc);
                }
            }
        }
    }

    /* Restore level styles after vault placement */
    styles_end_vault();

    /* Place dungeon monsters and objects */
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the grid */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" */
            if (*t == ' ')
                continue;

            /* Analyze the symbol */
            switch (*t)
            {
            /* A monster from 1 level deeper */
            case '1':
            {
                monster_level = p_ptr->depth + 1;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 2 levels deeper */
            case '2':
            {
                monster_level = p_ptr->depth + 2;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 3 levels deeper */
            case '3':
            {
                monster_level = p_ptr->depth + 3;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 4 levels deeper */
            case '4':
            {
                monster_level = p_ptr->depth + 4;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* An object from 1-4 levels deeper */
            case '*':
            {
                object_level = p_ptr->depth + dieroll(4);
                place_object(y, x, false, false, DROP_TYPE_NOT_DAMAGED);
                object_level = original_object_level;
                break;
            }

            /* A good object from 1-4 levels deeper */
            case '&':
            {
                object_level = p_ptr->depth + dieroll(4);
                place_object(y, x, true, false, DROP_TYPE_NOT_DAMAGED);
                object_level = original_object_level;
                break;
            }

            /* A chest from 4 levels deeper */
            case '~':
            {
                if (p_ptr->depth == 0)
                    object_level = MORGOTH_DEPTH;
                else
                    object_level = p_ptr->depth + 4;
                ;

                place_object(y, x, false, false, DROP_TYPE_CHEST);
                object_level = original_object_level;
                break;
            }

            /* A skeleton */
            case 'S':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                // make a skeleton 1/2 of the time
                if (one_in_(2))
                {
                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Wipe the object */
                    object_wipe(i_ptr);

                    if (one_in_(3))
                        k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
                    else
                        k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);

                    /* Prepare the item */
                    object_prep(i_ptr, k_idx);

                    i_ptr->pval = 1;

                    /* Drop it in the dungeon */
                    drop_near(i_ptr, -1, y, x);
                }
                break;
            }

            /* Monster and/or object from 1 level deeper */
            case '?':
            {
                int r = dieroll(3);

                if (r <= 2)
                {
                    monster_level = p_ptr->depth + 1;
                    place_monster(y, x, true, true, true);
                    monster_level = original_monster_level;
                }
                if (r >= 2)
                {
                    object_level = p_ptr->depth + 1;
                    place_object(y, x, false, false, DROP_TYPE_UNTHEMED);
                    object_level = original_object_level;
                }
                break;
            }

            /* Carcharoth */
            case 'C':
            {
                place_vault_monster_token('C', y, x);
                break;
            }

            /* silent watcher */
            case 'H':
            {
                place_vault_monster_token('H', y, x);
                break;
            }

            /* easterling spy */
            case '@':
            {
                place_vault_monster_token('@', y, x);
                break;
            }

            /* orc champion */
            case 'o':
            {
                place_vault_monster_token('o', y, x);
                break;
            }

            /* orc captain */
            case 'O':
            {
                place_vault_monster_token('O', y, x);
                break;
            }

            /* Tulkas Unclad */
            case 'P':
            {
                // Vault-based Tulkas spawning disabled - using room-based spawning only
                log_trace("Vault generation: Found 'P' character for Tulkas but vault spawning disabled");
                break;
            }

            case 'z':
            {
                /* Randomly spawn human or elf thrall */
                int thrall_r_idx = one_in_(2) ? R_IDX_HUMAN_THRALL : R_IDX_ELF_THRALL;
                place_monster_one(y, x, thrall_r_idx, true, true, NULL);
                break;
            }

            case 'Z':
            {
                place_vault_monster_token('Z', y, x);
                break;
            }

            /* cat warrior */
            case 'f':
            {
                place_vault_monster_token('f', y, x);
                break;
            }

            /* cat assassin */
            case 'F':
            {
                place_vault_monster_token('F', y, x);
                break;
            }

            /* troll guard */
            case 'T':
            {
                place_vault_monster_token('T', y, x);
                break;
            }

            /* barrow wight */
            case 'W':
            {
                place_vault_monster_token('W', y, x);
                break;
            }

            /* dragon */
            case 'd':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_DRAGON, true, p_ptr->depth + 4);
                break;
            }

            /* young cold drake */
            case 'y':
            {
                place_vault_monster_token('y', y, x);
                break;
            }

            /* young fire drake */
            case 'Y':
            {
                place_vault_monster_token('Y', y, x);
                break;
            }

            /* Spider */
            case 'M':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_SPIDER, true, p_ptr->depth + rand_range(1, 4));
                break;
            }

            /* Vampire */
            case 'v':
            {
                place_monster_by_letter(
                    y, x, 'v', true, p_ptr->depth + rand_range(1, 4));
                break;
            }

                /* Archer */
            case 'a':
            {
                place_monster_by_flag(
                    y, x, 4, (RF4_ARROW1 | RF4_ARROW2), true, p_ptr->depth + 1);
                break;
            }

                /* Flier */
            case 'b':
            {
                place_monster_by_flag(
                    y, x, 2, (RF2_FLYING), true, p_ptr->depth + 1);
                break;
            }

            /* Wolf */
            case 'c':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_WOLF, true, p_ptr->depth + rand_range(1, 4));
                break;
            }

            /* Rauko */
            case 'r':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_RAUKO, true, p_ptr->depth + rand_range(1, 4));
                break;
            }

                /* Aldor */
            case 'A':
            {
                place_vault_monster_token('A', y, x);
                break;
            }
            /* Aule (quest giver) */
            case 'L':
            {
                place_vault_monster_token('L', y, x);
                break;
            }
            /* Mandos (quest giver) */
            case 'N':
            {
                place_vault_monster_token('N', y, x);
                break;
            }

            /* Glaurung */
            case 'D':
            {
                place_vault_monster_token('D', y, x);
                break;
            }

            /* Gothmog */
            case 'R':
            {
                place_vault_monster_token('R', y, x);
                break;
            }

            /* Ungoliant */
            case 'U':
            {
                place_vault_monster_token('U', y, x);
                break;
            }

            /* Gorthaur */
            case 'G':
            {
                place_vault_monster_token('G', y, x);
                break;
            }

            /* Morgoth */
            case 'V':
            {
                place_vault_monster_token('V', y, x);
                break;
            }
            
            /* Duruin (Least of the Balrogs) */
            case 'B':
            {
                place_vault_monster_token('B', y, x);
                break;
            }
            
            /* Whispering shadow */
            case 'q':
            {
                place_vault_monster_token('q', y, x);
                break;
            }
            
            /* Shadow spider */
            case 'j':
            {
                place_vault_monster_token('j', y, x);
                break;
            }
            
            /* Lurking horror */
            case 'k':
            {
                place_vault_monster_token('k', y, x);
                break;
            }
            
            /* Nightthorn */
            case 'n':
            {
                place_vault_monster_token('n', y, x);
                break;
            }
            }
        }
    }

    /* Place dungeon features and objects */
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the location */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" */
            if (*t == ' ')
                continue;

            // some vaults are always lit
            if (v_ptr->flags & (VLT_LIGHT))
            {
                cave_info[y][x] |= (CAVE_GLOW);
            }

            // traps are usually 5 times as likely in vaults, but are 10 times
            // as likely if the TRAPS flag is set
            multiplier = (v_ptr->flags & (VLT_TRAPS)) ? 10 : 5;

            // another chance to place traps, with 4 times the normal chance
            // so traps in interesting rooms and vaults are a total of 5 times
            // more likely webbed vaults also have a large chance of receiving
            // webs
            if ((v_ptr->flags & (VLT_WEBS)))
            {
                if (cave_naked_bold(y, x) && one_in_(20))
                {
                    /* Place a web trap */
                    cave_set_feat(y, x, FEAT_TRAP_WEB);

                    // Hide it half the time
                    if (one_in_(2))
                    {
                        cave_info[y][x] |= (CAVE_HIDDEN);
                    }
                }
            }
            else if (dieroll(1000)
                <= trap_placement_chance(y, x) * (multiplier - 1))
            {
                place_trap(y, x);
            }
        }
    }

    log_trace("build_vault: Successfully built vault '%s' at (%d,%d)", v_name + v_ptr->name, y0, x0);
    return (true);
}

/*
 * Generate helper -- test a rectangle to see if it is all rock with reduced padding
 * (i.e. not floor and not icky) - used for quest vaults to reduce placement failures
 */
static bool solid_rock_reduced_padding(int y1, int x1, int y2, int x2)
{
    int y, x;

    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT)
        return (false);

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
                return (false);
            if (cave_info[y][x] & CAVE_ICKY)
                return (false);
        }
    }
    return (true);
}

/*
 * Place a room using forced placement strategy with reduced padding for quest vaults
 */
static bool place_room_forced(int y0, int x0, vault_type* v_ptr)
{
    int y1, x1, y2, x2;
    bool flip_d;
    
    log_trace("place_room_forced: Attempting to place quest vault '%s' at center (%d,%d), size %dx%d with reduced padding", 
             v_name + v_ptr->name, y0, x0, v_ptr->hgt, v_ptr->wid);

    // choose whether to rotate (flip diagonally)
    flip_d = one_in_(3);

    // some vaults ask not be be rotated
    if (v_ptr->flags & (VLT_NO_ROTATION))
        flip_d = false;

    if (flip_d)
    {
        /* determine the coordinates with height/width flipped */
        y1 = y0 - (v_ptr->wid / 2);
        x1 = x0 - (v_ptr->hgt / 2);
        y2 = y1 + v_ptr->wid - 1;
        x2 = x1 + v_ptr->hgt - 1;
    }

    else
    {
        /* determine the coordinates */
        y1 = y0 - (v_ptr->hgt / 2);
        x1 = x0 - (v_ptr->wid / 2);
        y2 = y1 + v_ptr->hgt - 1;
        x2 = x1 + v_ptr->wid - 1;
    }

    /* make sure that the location is within the map bounds */
    if ((y1 <= 2) || (x1 <= 2) || (y2 >= p_ptr->cur_map_hgt - 2)
        || (x2 >= p_ptr->cur_map_wid - 2))
    {
        log_trace("place_room_forced: Vault bounds check failed - y1=%d x1=%d y2=%d x2=%d (map size %dx%d)", 
                 y1, x1, y2, x2, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        return (false);
    }
    /* make sure that the location is empty using reduced padding (1 cell instead of 2) */
    if (!solid_rock_reduced_padding(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
    {
        log_trace("place_room_forced: solid_rock_reduced_padding check failed - area not empty around (%d,%d)-(%d,%d)", 
                 y1 - 1, x1 - 1, y2 + 1, x2 + 1);
        return (false);
    }

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, flip_d))
    {
        log_trace("place_room_forced: build_vault failed for quest vault '%s' at (%d,%d)", 
                 v_name + v_ptr->name, y0, x0);
        return (false);
    }
    
    log_trace("place_room_forced: Successfully built quest vault '%s' at (%d,%d) with reduced padding", 
             v_name + v_ptr->name, y0, x0);

    /* save the corner locations */
    dun->corner[dun->cent_n].y1 = y1 + 1;
    dun->corner[dun->cent_n].x1 = x1 + 1;
    dun->corner[dun->cent_n].y2 = y2 - 1;
    dun->corner[dun->cent_n].x2 = x2 - 1;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = (byte)v_ptr->typ;
    dun->is_quest[dun->cent_n] = (v_ptr->flags & VLT_QUEST) ? true : false;
    dun->cent_n++;

    /* Cause a special feeling */
    good_item_flag = true;

    log_trace("build_vault: *** SUCCESSFULLY COMPLETED *** vault '%s' at (%d,%d)", 
              v_name + v_ptr->name, y0, x0);
    
    /* DEBUGGING: For quest vaults, do immediate verification */
    if (v_ptr->flags & VLT_QUEST) {
        int verify_y1 = y0 - v_ptr->hgt / 2;
        int verify_x1 = x0 - v_ptr->wid / 2;
        int verify_y2 = verify_y1 + v_ptr->hgt - 1;
        int verify_x2 = verify_x1 + v_ptr->wid - 1;
        
        int post_walls = 0, post_floors = 0, post_features = 0, post_monsters = 0;
        int post_icky = 0, post_room = 0;
        
        for (int vy = verify_y1; vy <= verify_y2; vy++) {
            for (int vx = verify_x1; vx <= verify_x2; vx++) {
                if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                    post_walls++;
                } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                    post_floors++;
                } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                    post_features++;
                }
                
                if (cave_m_idx[vy][vx] > 0) {
                    post_monsters++;
                }
                
                if (cave_info[vy][vx] & CAVE_ICKY) {
                    post_icky++;
                }
                
                if (cave_info[vy][vx] & CAVE_ROOM) {
                    post_room++;
                }
            }
        }
        
        log_trace("build_vault: QUEST VAULT POST-BUILD VERIFICATION: Area (%d,%d) to (%d,%d)", 
                  verify_y1, verify_x1, verify_y2, verify_x2);
        log_trace("build_vault: QUEST VAULT POST-BUILD: %d walls, %d floors, %d features, %d monsters", 
                  post_walls, post_floors, post_features, post_monsters);
        log_trace("build_vault: QUEST VAULT POST-BUILD: %d CAVE_ICKY, %d CAVE_ROOM flags", 
                  post_icky, post_room);
    }

    return (true);
}

static bool place_room(int y0, int x0, vault_type* v_ptr)
{
    int y1, x1, y2, x2;
    bool flip_d;
    
    log_trace("place_room: Attempting to place vault '%s' at center (%d,%d), size %dx%d", 
             v_name + v_ptr->name, y0, x0, v_ptr->hgt, v_ptr->wid);

    // choose whether to rotate (flip diagonally)
    flip_d = one_in_(3);

    // some vaults ask not be be rotated
    if (v_ptr->flags & (VLT_NO_ROTATION))
        flip_d = false;

    if (flip_d)
    {
        /* determine the coordinates with height/width flipped */
        y1 = y0 - (v_ptr->wid / 2);
        x1 = x0 - (v_ptr->hgt / 2);
        y2 = y1 + v_ptr->wid - 1;
        x2 = x1 + v_ptr->hgt - 1;
    }

    else
    {
        /* determine the coordinates */
        y1 = y0 - (v_ptr->hgt / 2);
        x1 = x0 - (v_ptr->wid / 2);
        y2 = y1 + v_ptr->hgt - 1;
        x2 = x1 + v_ptr->wid - 1;
    }

    /* make sure that the location is within the map bounds */
    if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
        || (x2 >= p_ptr->cur_map_wid - 3))
    {
        log_trace("place_room: Vault bounds check failed - y1=%d x1=%d y2=%d x2=%d (map size %dx%d)", 
                 y1, x1, y2, x2, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        return (false);
    }
    /* make sure that the location is empty */
    if (!solid_rock(y1 - 2, x1 - 2, y2 + 2, x2 + 2))
    {
        log_trace("place_room: solid_rock check failed - area not empty around (%d,%d)-(%d,%d)", 
                 y1 - 2, x1 - 2, y2 + 2, x2 + 2);
        return (false);
    }

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, flip_d))
    {
        log_trace("place_room: build_vault failed for vault '%s' at (%d,%d)", 
                 v_name + v_ptr->name, y0, x0);
        return (false);
    }
    
    log_trace("place_room: Successfully built vault '%s' at (%d,%d)", 
             v_name + v_ptr->name, y0, x0);

    /* save the corner locations */
    dun->corner[dun->cent_n].y1 = y1 + 1;
    dun->corner[dun->cent_n].x1 = x1 + 1;
    dun->corner[dun->cent_n].y2 = y2 - 1;
    dun->corner[dun->cent_n].x2 = x2 - 1;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = (byte)v_ptr->typ;
    dun->is_quest[dun->cent_n] = (v_ptr->flags & VLT_QUEST) ? true : false;
    dun->cent_n++;

    /* Cause a special feeling */
    good_item_flag = true;

    return (true);
}

typedef enum vault_dock_dir
{
    VAULT_DOCK_NORTH = 0,
    VAULT_DOCK_EAST = 1,
    VAULT_DOCK_SOUTH = 2,
    VAULT_DOCK_WEST = 3
} vault_dock_dir_t;

/* Ensure we have clear granite around a prospective docked vault, allowing
 * the contact edge to abut an existing vault wall. */
static bool area_clear_for_vault_dock(
    int y1, int x1, int y2, int x2, vault_dock_dir_t dir)
{
    int y_lo = y1 - 1;
    int y_hi = y2 + 1;
    int x_lo = x1 - 1;
    int x_hi = x2 + 1;

    if ((y_lo < 1) || (x_lo < 1) || (y_hi >= p_ptr->cur_map_hgt - 1)
        || (x_hi >= p_ptr->cur_map_wid - 1))
    {
        return false;
    }

    for (int y = y_lo; y <= y_hi; ++y)
    {
        for (int x = x_lo; x <= x_hi; ++x)
        {
            bool on_contact = false;
            switch (dir)
            {
            case VAULT_DOCK_EAST:
                on_contact = (x == x1 - 1);
                break;
            case VAULT_DOCK_WEST:
                on_contact = (x == x2 + 1);
                break;
            case VAULT_DOCK_NORTH:
                on_contact = (y == y2 + 1);
                break;
            case VAULT_DOCK_SOUTH:
                on_contact = (y == y1 - 1);
                break;
            }

            if (on_contact)
            {
                /* Allow touching an existing vault wall, but not overlapping
                 * known open space such as corridors. */
                if ((cave_feat[y][x] == FEAT_FLOOR)
                    && !(cave_info[y][x] & (CAVE_ICKY)))
                {
                    return false;
                }
                continue;
            }

            if (cave_info[y][x] & (CAVE_ROOM | CAVE_ICKY))
            {
                return false;
            }

            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
            {
                return false;
            }
        }
    }

    return true;
}

/* Pick a contact point along one edge of an existing vault, preferring doors
 * but falling back to plain walls. */
static bool choose_vault_contact(
    int base_idx, vault_dock_dir_t dir, int* y_out, int* x_out)
{
    int y1 = dun->corner[base_idx].y1 - 1;
    int y2 = dun->corner[base_idx].y2 + 1;
    int x1 = dun->corner[base_idx].x1 - 1;
    int x2 = dun->corner[base_idx].x2 + 1;

    int door_seen = 0, wall_seen = 0;
    int door_y = 0, door_x = 0, wall_y = 0, wall_x = 0;

    if (dir == VAULT_DOCK_EAST || dir == VAULT_DOCK_WEST)
    {
        int x = (dir == VAULT_DOCK_EAST) ? x2 : x1;
        for (int y = y1 + 1; y <= y2 - 1; ++y)
        {
            if (!(cave_info[y][x] & (CAVE_ICKY)))
                continue;
            int feat = cave_feat[y][x];
            if (feature_is_any_door(feat))
            {
                door_seen++;
                if (one_in_(door_seen))
                {
                    door_y = y;
                    door_x = x;
                }
            }
            else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
            {
                wall_seen++;
                if (one_in_(wall_seen))
                {
                    wall_y = y;
                    wall_x = x;
                }
            }
        }
    }
    else
    {
        int y = (dir == VAULT_DOCK_NORTH) ? y1 : y2;
        for (int x = x1 + 1; x <= x2 - 1; ++x)
        {
            if (!(cave_info[y][x] & (CAVE_ICKY)))
                continue;
            int feat = cave_feat[y][x];
            if (feature_is_any_door(feat))
            {
                door_seen++;
                if (one_in_(door_seen))
                {
                    door_y = y;
                    door_x = x;
                }
            }
            else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
            {
                wall_seen++;
                if (one_in_(wall_seen))
                {
                    wall_y = y;
                    wall_x = x;
                }
            }
        }
    }

    if (door_seen > 0)
    {
        *y_out = door_y;
        *x_out = door_x;
        return true;
    }
    if (wall_seen > 0)
    {
        *y_out = wall_y;
        *x_out = wall_x;
        return true;
    }
    return false;
}

/* Attempt to place a vault flush against an existing vault so that a single
 * door separates them. Returns the placed centre if successful. */
static bool try_place_docked_vault(
    vault_type* v_ptr, int* out_y0, int* out_x0)
{
    if (!room_kind_is_vault((byte)v_ptr->typ))
    {
        return false;
    }

    if (v_ptr->flags & (VLT_QUEST))
    {
        return false;
    }

    if (dun->cent_n >= CENT_MAX)
    {
        return false;
    }

    /* Collect existing vault indices to target */
    int vault_indices[CENT_MAX];
    int vault_count = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (room_kind_is_vault(dun->kind[i]) && !dun->is_quest[i])
        {
            vault_indices[vault_count++] = i;
        }
    }
    if (vault_count == 0)
    {
        return false;
    }

    /* Try a handful of random attachment attempts */
    for (int attempt = 0; attempt < 40; ++attempt)
    {
        styles_set_vault_avoid_style(-1);
        int base_idx = vault_indices[rand_int(vault_count)];
        int base_y1 = dun->corner[base_idx].y1 - 1;
        int base_y2 = dun->corner[base_idx].y2 + 1;
        int base_x1 = dun->corner[base_idx].x1 - 1;
        int base_x2 = dun->corner[base_idx].x2 + 1;

        vault_dock_dir_t dir_order[4] = {VAULT_DOCK_NORTH, VAULT_DOCK_EAST,
            VAULT_DOCK_SOUTH, VAULT_DOCK_WEST};
        for (int s = 0; s < 4; ++s)
        {
            int swap_idx = rand_int(4);
            vault_dock_dir_t tmp = dir_order[s];
            dir_order[s] = dir_order[swap_idx];
            dir_order[swap_idx] = tmp;
        }

        for (int di = 0; di < 4; ++di)
        {
            vault_dock_dir_t dir = dir_order[di];
            int contact_y = 0, contact_x = 0;
            if (!choose_vault_contact(base_idx, dir, &contact_y, &contact_x))
            {
                continue;
            }

            /* Prefer a different primary style than the contacted vault */
            int avoid_style = style_at_color(contact_y, contact_x);
            styles_set_vault_avoid_style(avoid_style);

            bool flip_d = (!(v_ptr->flags & (VLT_NO_ROTATION)) && one_in_(3));
            int h = flip_d ? v_ptr->wid : v_ptr->hgt;
            int w = flip_d ? v_ptr->hgt : v_ptr->wid;

            int y0 = 0, x0 = 0, y1 = 0, x1 = 0, y2 = 0, x2 = 0;

            switch (dir)
            {
            case VAULT_DOCK_EAST:
                x1 = base_x2 + 1;
                x2 = x1 + w - 1;
                x0 = x1 + w / 2;
                y0 = contact_y;
                y1 = y0 - h / 2;
                y2 = y1 + h - 1;
                break;
            case VAULT_DOCK_WEST:
                x2 = base_x1 - 1;
                x1 = x2 - w + 1;
                x0 = x1 + w / 2;
                y0 = contact_y;
                y1 = y0 - h / 2;
                y2 = y1 + h - 1;
                break;
            case VAULT_DOCK_NORTH:
                y2 = base_y1 - 1;
                y1 = y2 - h + 1;
                y0 = y1 + h / 2;
                x0 = contact_x;
                x1 = x0 - w / 2;
                x2 = x1 + w - 1;
                break;
            case VAULT_DOCK_SOUTH:
                y1 = base_y2 + 1;
                y2 = y1 + h - 1;
                y0 = y1 + h / 2;
                x0 = contact_x;
                x1 = x0 - w / 2;
                x2 = x1 + w - 1;
                break;
            }

            if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
                || (x2 >= p_ptr->cur_map_wid - 3))
            {
                continue;
            }

            if (!area_clear_for_vault_dock(y1, x1, y2, x2, dir))
            {
                continue;
            }

            if (!build_vault(y0, x0, v_ptr, flip_d))
            {
                styles_set_vault_avoid_style(-1);
                continue;
            }

            dun->corner[dun->cent_n].y1 = y1 + 1;
            dun->corner[dun->cent_n].x1 = x1 + 1;
            dun->corner[dun->cent_n].y2 = y2 - 1;
            dun->corner[dun->cent_n].x2 = x2 - 1;
            dun->cent[dun->cent_n].y = y0;
            dun->cent[dun->cent_n].x = x0;
            dun->kind[dun->cent_n] = (byte)v_ptr->typ;
            dun->is_quest[dun->cent_n] = false;
            int new_idx = dun->cent_n;
            dun->cent_n++;

            dun->connection[base_idx][new_idx] = true;
            dun->connection[new_idx][base_idx] = true;

            int new_y = contact_y;
            int new_x = contact_x;
            if (dir == VAULT_DOCK_EAST)
                new_x = contact_x + 1;
            else if (dir == VAULT_DOCK_WEST)
                new_x = contact_x - 1;
            else if (dir == VAULT_DOCK_SOUTH)
                new_y = contact_y + 1;
            else
                new_y = contact_y - 1;

            if (!feature_is_any_door(cave_feat[contact_y][contact_x]))
            {
                place_closed_door(contact_y, contact_x);
            }
            cave_set_feat(new_y, new_x, FEAT_FLOOR);

            good_item_flag = true;

            if (out_y0)
                *out_y0 = y0;
            if (out_x0)
                *out_x0 = x0;

            styles_set_vault_avoid_style(-1);
            return true;
        }
    }

    styles_set_vault_avoid_style(-1);
    return false;
}

/*
 * Type 6 -- least vaults (see "vault.txt")
 */
/* Helper: scan vault template text (from vault.txt) for Aule symbol 'L' BEFORE placement */
static bool vault_template_has_aule(vault_type *v) {
    if (!v || v->text == 0 || v->hgt == 0) return false;
    char *s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row) {
        if (strchr(s, 'L')) return true; /* 'L' designates Aule in template */
        s += strlen(s) + 1; /* advance to next stored line (null-terminated) */
    }
    return false;
}

static bool vault_template_has_mandos(vault_type *v) {
    if (!v || v->text == 0 || v->hgt == 0) return false;
    char *s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row) {
        if (strchr(s, 'N')) return true; /* 'N' designates Mandos in template */
        s += strlen(s) + 1; /* advance to next stored line (null-terminated) */
    }
    return false;
}

static bool vault_template_has_duruin(vault_type *v) {
    if (!v) return false;
    /* Check vault name directly - Duruin Bastion is vault ID 461 */
    const char *name = v_name + v->name;
    return (strstr(name, "Duruin") != NULL || strstr(name, "Bastion") != NULL);
}

/* Global variables to store quest vault coordinates for monitoring */
int qv_stored_y1 = -1, qv_stored_x1 = -1, qv_stored_y2 = -1, qv_stored_x2 = -1;
bool qv_placed_this_level = false;  /* Track if quest vault actually placed this level */

/* DEBUGGING: Function to check if quest vault still exists at monitored coordinates */
static void check_quest_vault_integrity(const char* checkpoint_name) {
    if (!qv_placed_this_level) {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault placed this level - skipping check", checkpoint_name);
        return;
    }
    if (qv_stored_y1 < 0 || qv_stored_y2 < 0) {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault coordinates stored", checkpoint_name);
        return;
    }
    
    int check_walls = 0, check_floors = 0, check_features = 0, check_monsters = 0;
    int check_icky = 0, check_room = 0, check_extra = 0;
    
    for (int cy = qv_stored_y1; cy <= qv_stored_y2; cy++) {
        for (int cx = qv_stored_x1; cx <= qv_stored_x2; cx++) {
            if (cave_feat[cy][cx] == FEAT_WALL_OUTER || cave_feat[cy][cx] == FEAT_WALL_INNER) {
                check_walls++;
            } else if (cave_feat[cy][cx] == FEAT_FLOOR) {
                check_floors++;
            } else if (cave_feat[cy][cx] == FEAT_WALL_EXTRA) {
                check_extra++;
            } else {
                check_features++;
            }
            
            if (cave_m_idx[cy][cx] > 0) {
                check_monsters++;
            }
            
            if (cave_info[cy][cx] & CAVE_ICKY) {
                check_icky++;
            }
            
            if (cave_info[cy][cx] & CAVE_ROOM) {
                check_room++;
            }
        }
    }
    
    log_trace("VAULT INTEGRITY CHECK [%s]: Area (%d,%d) to (%d,%d)", 
              checkpoint_name, qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d walls, %d floors, %d features, %d monsters, %d extra_walls", 
              checkpoint_name, check_walls, check_floors, check_features, check_monsters, check_extra);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d CAVE_ICKY, %d CAVE_ROOM flags", 
              checkpoint_name, check_icky, check_room);
              
    /* Alert if vault appears to be gone */
    if (check_walls < 50 && check_floors < 30) {
        log_trace("VAULT INTEGRITY WARNING [%s]: Vault appears to have been OVERWRITTEN! Very low content.", checkpoint_name);
    }
}

static void process_quest_vault_area(int y0, int x0, vault_type *qv) {
    int y1 = y0 - qv->hgt / 2;
    int x1 = x0 - qv->wid / 2;
    int y2 = y1 + qv->hgt - 1;
    int x2 = x1 + qv->wid - 1;
    bool has_forge = false;
    bool has_aule  = false;
    bool has_mandos = false;
    
    log_trace("Quest vault processing: Area (%d,%d) to (%d,%d), size %dx%d", 
              y1, x1, y2, x2, qv->wid, qv->hgt);
    
    /* Debug: Check what's actually in the vault area */
    int wall_count = 0, floor_count = 0, monster_count = 0, feature_count = 0;
    for (int dy = y1; dy <= y2; ++dy) {
        for (int dx = x1; dx <= x2; ++dx) {
            if (cave_feat[dy][dx] == FEAT_WALL_OUTER || cave_feat[dy][dx] == FEAT_WALL_INNER) {
                wall_count++;
            } else if (cave_feat[dy][dx] == FEAT_FLOOR) {
                floor_count++;
            } else if (cave_feat[dy][dx] != FEAT_WALL_EXTRA) {
                feature_count++;
            }
            
            if (cave_m_idx[dy][dx] > 0) {
                monster_count++;
            }
            
            if ((cave_feat[dy][dx] >= FEAT_FORGE_HEAD) && (cave_feat[dy][dx] <= FEAT_FORGE_TAIL)) {
                if (!has_forge) {
                    p_ptr->aule_forge_y = (byte)dy;
                    p_ptr->aule_forge_x = (byte)dx;
                    has_forge = true;
                    log_trace("Quest vault: Found forge at (%d,%d), feature=%d", dy, dx, cave_feat[dy][dx]);
                }
            }
            if (cave_m_idx[dy][dx] > 0) {
                monster_type *m_ptr = &mon_list[cave_m_idx[dy][dx]];
                if (m_ptr->r_idx == R_IDX_AULE) {
                    has_aule = true;
                    log_trace("Quest vault: Found Aule at (%d,%d)", dy, dx);
                }
                if (m_ptr->r_idx == R_IDX_MANDOS) {
                    has_mandos = true;
                    p_ptr->mandos_vault_y = (byte)dy;
                    p_ptr->mandos_vault_x = (byte)dx;
                    log_trace("Quest vault: Found Mandos at (%d,%d)", dy, dx);
                }
            }
        }
    }
    
    log_trace("Quest vault contents: %d walls, %d floors, %d features, %d monsters", 
              wall_count, floor_count, feature_count, monster_count);
              
    /* DEBUGGING: Store quest vault bounds for monitoring */
    qv_stored_y1 = y1; qv_stored_x1 = x1; qv_stored_y2 = y2; qv_stored_x2 = x2;
    qv_placed_this_level = true;  /* Mark that quest vault was actually placed */
    log_trace("QUEST VAULT MONITOR: Storing bounds (%d,%d) to (%d,%d) for tracking", 
              qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2);
              
#if DEBUG_QUEST_VAULT
    qv_y1 = y1; qv_x1 = x1; qv_y2 = y2; qv_x2 = x2; /* record bounds */
    qv_capture();
    qv_dump("initial");
    /* Force mark/reveal for debugging */
    for (int ry = y1; ry <= y2; ++ry) for (int rx = x1; rx <= x2; ++rx) cave_info[ry][rx] |= (CAVE_MARK|CAVE_SEEN|CAVE_GLOW);
#endif
    if (has_forge && has_aule && p_ptr->aule_quest == AULE_QUEST_NOT_STARTED && 
        !quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE) && !p_ptr->quest_reserved[0]) {
        /* Immediately reserve quest slot to prevent other quests from spawning */
        p_ptr->quest_reserved[0] = 1;
        /* Record pending quest state change instead of applying immediately */
        pending_quest_states.has_aule_change = true;
        pending_quest_states.aule_level = p_ptr->depth;
        pending_quest_states.aule_forge_y = p_ptr->aule_forge_y;
        pending_quest_states.aule_forge_x = p_ptr->aule_forge_x;
        log_trace("Aule quest: FORGE_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d, quest_reserved[0] set to 1", p_ptr->aule_forge_y, p_ptr->aule_forge_x, p_ptr->depth);
    }
    if (has_mandos && p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED && 
        !quest_metarun_blocked(QUEST_ID_MANDOS, METARUN_QUEST_MANDOS) && !p_ptr->quest_reserved[0]) {
        /* Immediately reserve quest slot to prevent other quests from spawning */
        p_ptr->quest_reserved[0] = 1;
        /* Record pending quest state change instead of applying immediately */
        pending_quest_states.has_mandos_change = true;
        pending_quest_states.mandos_level = p_ptr->depth;
        pending_quest_states.mandos_vault_y = p_ptr->mandos_vault_y;
        pending_quest_states.mandos_vault_x = p_ptr->mandos_vault_x;
        log_trace("Mandos quest: GIVER_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d, quest_reserved[0] set to 1", p_ptr->mandos_vault_y, p_ptr->mandos_vault_x, p_ptr->depth);
    }
}

static bool build_type6(int y0, int x0, bool force_forge)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick an interesting room */
    while (true)
    {
        unsigned long long rarity = 0;
        tries++;

        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        // log_trace("Vault selection: Trying vault #%d '%s' (type=%d, depth=%d, rarity=%d, flags=0x%x)",
        //           (int)(v_ptr - v_info), v_name + v_ptr->name, v_ptr->typ, v_ptr->depth, v_ptr->rarity, v_ptr->flags);

        // if forcing a forge, then skip vaults without forges in them
        if (force_forge && !v_ptr->forge)
        {
            log_trace("Skipping vault - force_forge=true but vault has no forge");
            continue;
        }

        // unless forcing a forge, try additional times to place any vault
        // marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST))
            && !p_ptr->force_forge)
        {
            // log_trace("Skipping vault - tries=%d, no TEST flag", tries);
            continue;
        }

        rarity = v_ptr->rarity;
        if (p_ptr->depth < 6)
        {
            /* Surface rooms are more common at low depths */
            if (!(v_ptr->flags & (VLT_SURFACE)) && !one_in_(4))
                continue;
        }
        else if (v_ptr->flags & (VLT_SURFACE))
        {
            /* Surface rooms get very much rarer at depth */
            rarity += (1 << (p_ptr->depth));
        }

        /* Accept the first interesting room (but not quest vaults) */
        if ((v_ptr->typ == 6) && (v_ptr->depth <= p_ptr->depth)
            && (one_in_(rarity)) && !(v_ptr->flags & VLT_QUEST))
            break;

        if (tries > 20000)
        {
            if (!DEPLOYMENT || cheat_room)
                msg_format(
                    "Bug: Could not find a record for an Interesting Room in "
                    "vault.txt");
            return (false);
        }
    }

    if (!force_forge && one_in_(4))
    {
        if (try_place_docked_vault(v_ptr, NULL, NULL))
        {
            return true;
        }
    }

    return place_room(y0, x0, v_ptr);
}

/*
 * Type 7 -- lesser vaults (see "vault.txt")
 */
static bool build_type7(int y0, int x0)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick a lesser vault */
    while (true)
    {
        tries++;

        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        // try additional times to place any vault marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST)))
            continue;

        /* Accept the first lesser vault (but not quest vaults) */
        if ((v_ptr->typ == 7) && (v_ptr->depth <= p_ptr->depth)
            && (one_in_(v_ptr->rarity)) && !(v_ptr->flags & VLT_QUEST))
            break;

        if (tries > 2000)
        {
            msg_format(
                "Bug: Could not find a record for a Lesser Vault in vault.txt");
            return (false);
        }
    }

    bool placed = false;
    int placed_y = y0, placed_x = x0;

    if (one_in_(4) && try_place_docked_vault(v_ptr, &placed_y, &placed_x))
    {
        placed = true;
    }
    else if (place_room(y0, x0, v_ptr))
    {
        placed = true;
    }

    if (!placed)
        return false;
    /* Message */
    if (cheat_room)
        msg_format("LV (%s).", v_name + v_ptr->name);

    return true;
}

/*
 * Mark greater vault grids with the CAVE_G_VAULT flag.
 * Returns true if it succeds.
 */
static bool mark_g_vault(int y0, int x0, int ymax, int xmax)
{
    int y1, x1, y2, x2, y, x;

    /* Get the coordinates */
    y1 = y0 - ymax / 2;
    x1 = x0 - xmax / 2;
    y2 = y1 + ymax - 1;
    x2 = x1 + xmax - 1;

    /* Step 1 - Mark all grids inside that perimeter with the new flag */
    for (y = y1 + 1; y < y2; y++)
    {
        for (x = x1 + 1; x < x2; x++)
        {
            cave_info[y][x] |= (CAVE_G_VAULT);
        }
    }

    return (true);
}

/*
 * Type 8 -- greater vaults (see "vault.txt")
 */
static bool build_type8(int y0, int x0)
{
    vault_type* v_ptr = NULL;
    int tries = 0;
    bool found = false;
    bool repeated = false;
    int i;
    s16b v_idx;

    // Can only have one greater vault per level
    if (g_vault_name[0] != '\0')
    {
        return (false);
    }

    /* Pick a greater vault */
    while (!found)
    {
        tries++;

        /* Get a random vault record */
        v_idx = rand_int(z_info->v_max);
        v_ptr = &v_info[v_idx];

        // try additional times to place any vault marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST)))
            continue;

        /* Accept the first greater vault (but not quest vaults) */
        if ((v_ptr->typ == 8) && (v_ptr->depth <= p_ptr->depth)
            && (one_in_(v_ptr->rarity)) && !(v_ptr->flags & VLT_QUEST))
        {
            repeated = false;
            for (i = 0; i < MAX_GREATER_VAULTS; i++)
            {
                if (v_idx == p_ptr->greater_vaults[i])
                {
                    repeated = true;
                }
            }

            if (!repeated)
                found = true;
        }

        if (tries > 2000)
        {
            // if (!repeated) msg_debug("Bug: Could not find a record for a
            // Greater Vault in vault.txt");
            return (false);
        }
    }

    bool placed = false;
    int placed_y = y0, placed_x = x0;
    if (one_in_(2) && try_place_docked_vault(v_ptr, &placed_y, &placed_x))
    {
        placed = true;
    }
    else if (place_room(y0, x0, v_ptr))
    {
        placed = true;
    }

    if (!placed)
        return false;

    // Remember this greater vault
    for (i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        if (p_ptr->greater_vaults[i] == 0)
        {
            p_ptr->greater_vaults[i] = v_idx;
            break;
        }
    }

    /* Message */
    if (cheat_room)
        msg_format("GV (%s).", v_name + v_ptr->name);

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(placed_y, placed_x, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }
    return (true);
}

/*
 * Type 9 -- Morgoth's vault (see "vault.txt")
 */
static bool build_type9(int y0, int x0)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick a version of Morgoth's vault */
    while (true)
    {
        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        /* Accept the first morgoth vault */
        if (v_ptr->typ == 9)
            break;

        tries++;
        if (tries > 10000)
        {
            msg_format(
                "Could not find a record for Morgoth's Vault in vault.txt");
            return (false);
        }
    }

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
    {
        return (false);
    }

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return (true);
}

/*
 * Type 10 -- The Gates of Angband (see "vault.txt")
 */
static bool build_type10(int y0, int x0)
{
    vault_type* v_ptr;

    /* Get the first vault record */
    v_ptr = &v_info[1];

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
    {
        return (false);
    }

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return (true);
}

/*
 * Attempt to build a room of the given type at the given co-ordinates
 */
static bool room_build(int typ)
{
    int y, x;

    if (dun->cent_n >= CENT_MAX)
    {
        return (false);
    }

    y = rand_range(5, p_ptr->cur_map_hgt - 5);
    x = rand_range(5, p_ptr->cur_map_wid - 5);

    /* Build a room */
    switch (typ)
    {
    /* Build an appropriate room */
    // Greater Vault
    case 8:
    {
        if (!build_type8(y, x))
        {
            return (false);
        }
        break;
    }
    // Lesser Vault
    case 7:
    {
        if (!build_type7(y, x))
        {
            return (false);
        }
        break;
    }
    // Least Vault
    case 6:
    {
        if (!build_type6(y, x, false))
        {
            return (false);
        }
        break;
    }
    // Cross Room
    case 2:
    {
        if (!build_type2(y, x))
        {
            return (false);
        }
        break;
    }
    // Normal Room
    case 1:
    {
        if (!build_type1(y, x))
        {
            return (false);
        }
        break;
    }
    /* Paranoia */
    default:
        return (false);
    }

    /* Success */
    return (true);
}

/*
 * Try to place a quest vault of specified type using forced placement strategy
 * Returns true if successfully placed, false otherwise
 */
static bool place_duruin_bastion(void)
{
    vault_type* qv_ptr;
    int y, x;
    
    log_trace("Varda quest: Attempting to force-place Duruin Bastion at depth %d", p_ptr->depth);
    
    for (int i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_has_duruin(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        
        /* Found Duruin Bastion - attempt placement and return result */
        log_trace("Varda quest: Found Duruin Bastion vault at index %d: '%s', attempting placement", i, v_name + qv_ptr->name);
        log_trace("Varda quest: Vault details - typ=%d, hgt=%d, wid=%d, depth=%d, flags=0x%x", 
                  qv_ptr->typ, qv_ptr->hgt, qv_ptr->wid, qv_ptr->depth, qv_ptr->flags);
        
        /* Reserve quest slot immediately to prevent other quest spawning during placement */
        p_ptr->quest_reserved[0] = 1;
        
        int center_y = p_ptr->cur_map_hgt / 2;
        int center_x = p_ptr->cur_map_wid / 2;
        
        /* Attempt primary placement near center */
        y = center_y + rand_range(-p_ptr->cur_map_hgt/6, p_ptr->cur_map_hgt/6);
        x = center_x + rand_range(-p_ptr->cur_map_wid/6, p_ptr->cur_map_wid/6);
        y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
        x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
        
        if (place_room_forced(y, x, qv_ptr)) {
            qv_placed_this_level = true;
            process_quest_vault_area(y, x, qv_ptr);
            pending_quest_states.has_varda_change = true;
            pending_quest_states.varda_level = p_ptr->depth;
            pending_quest_states.varda_vault_y = y;
            pending_quest_states.varda_vault_x = x;
            log_trace("Varda quest: Duruin Bastion placed at (%d,%d)", y, x);
            return true;
        }
        
        /* Fallback attempts with wider variance */
        for (int attempts = 0; attempts < 10; attempts++) {
            y = center_y + rand_range(-p_ptr->cur_map_hgt/4, p_ptr->cur_map_hgt/4);
            x = center_x + rand_range(-p_ptr->cur_map_wid/4, p_ptr->cur_map_wid/4);
            y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
            x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
            
            if (place_room_forced(y, x, qv_ptr)) {
                qv_placed_this_level = true;
                process_quest_vault_area(y, x, qv_ptr);
                pending_quest_states.has_varda_change = true;
                pending_quest_states.varda_level = p_ptr->depth;
                pending_quest_states.varda_vault_y = y;
                pending_quest_states.varda_vault_x = x;
                log_trace("Varda quest: Duruin Bastion placed on fallback attempt %d at (%d,%d)", attempts + 1, y, x);
                return true;
            }
        }
        
        /* If we reach here, Duruin placement failed - return immediately without trying other vaults */
        log_trace("Varda quest: Duruin Bastion placement failed after all attempts, returning false");
        return false;
    }
    
    log_trace("Varda quest: Failed to find Duruin Bastion vault template at depth %d", p_ptr->depth);
    return false;
}

static bool try_quest_vault_type(int v_type)
{
    int i;
    vault_type* qv_ptr;
    int y, x;
    
    log_trace("Quest vault: Attempting type %d quest vault with forced placement strategy", v_type);
    
    for (i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (qv_ptr->typ != v_type) continue;
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (vault_template_has_duruin(qv_ptr)) {
            log_trace("Quest vault: Skipping Duruin Bastion in generic placement path (quest-only)");
            continue;
        }
        
        log_trace("Quest vault: Checking vault %d '%s' (rarity=%d)", i, v_name + qv_ptr->name, qv_ptr->rarity);
        
        if (!one_in_(qv_ptr->rarity)) {
            log_trace("Quest vault: Rarity check failed (1/%d)", qv_ptr->rarity);
            continue;
        }
        
        /* Check Aule requirements */
        if (vault_template_has_aule(qv_ptr)) {
            log_trace("Quest vault: === AULE VAULT DETECTED === Checking eligibility (depth=%d)", p_ptr->depth);
            log_trace("Quest vault: CRITICAL CHECK - quest_reserved[0]=%d (MUST be 0 to proceed)", p_ptr->quest_reserved[0]);
            log_trace("  Player SMT skill_base = %d", p_ptr->skill_base[S_SMT]);
            log_trace("  Player SMT skill_use = %d", p_ptr->skill_use[S_SMT]);
            
            /* Use data-driven eligibility check from quest.txt E: field */
            if (!check_quest_eligibility(2, p_ptr->depth)) { /* Aule is quest index 2 */
                log_trace("Quest vault: Aule vault skipped (eligibility check failed)");
                continue;
            }
            log_trace("Quest vault: Aule eligibility check PASSED");
            
            if (quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE)) {
                log_trace("Quest vault: Aule vault skipped (quest blocked by metarun)");
                continue;
            }
            if (p_ptr->quest_reserved[0]) {
                log_trace("Quest vault: === AULE BLOCKED === Another quest already spawned (quest_reserved[0]=1)");
                continue;
            }
            log_trace("Quest vault: === AULE APPROVED === All checks passed, proceeding with generation");
        }
        
        /* Check Mandos requirements */
        if (vault_template_has_mandos(qv_ptr)) {
            log_trace("Quest vault: Checking Mandos vault '%s' - mandos_quest=%d, quest_reserved[0]=%d", 
                     v_name + qv_ptr->name, p_ptr->mandos_quest, p_ptr->quest_reserved[0]);
            if (p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED) {
                log_trace("Quest vault: Mandos vault skipped (quest state %d)", 
                         p_ptr->mandos_quest);
                continue;
            }
            if (quest_metarun_blocked(QUEST_ID_MANDOS, METARUN_QUEST_MANDOS)) {
                log_trace("Quest vault: Mandos vault skipped (quest blocked by metarun)");
                continue;
            }
            if (p_ptr->quest_reserved[0]) {
                log_trace("Quest vault: Mandos vault skipped (another quest already spawned this run)");
                continue;
            }
        }
        
        /* Reserve quest slot immediately to prevent other quest spawning during level generation */
        log_trace("Quest vault: Requirements passed for vault '%s', reserving quest slot", v_name + qv_ptr->name);
        p_ptr->quest_reserved[0] = 1;
        
        /* Use forced placement strategy like forge placement:
         * Pick optimal location near center and use reduced padding */
        
        /* Calculate optimal placement position (center of map with some variation) */
        int center_y = p_ptr->cur_map_hgt / 2;
        int center_x = p_ptr->cur_map_wid / 2;
        
        /* Add some randomness but keep near center for best chance of success */
        y = center_y + rand_range(-p_ptr->cur_map_hgt/6, p_ptr->cur_map_hgt/6);
        x = center_x + rand_range(-p_ptr->cur_map_wid/6, p_ptr->cur_map_wid/6);
        
        /* Ensure within reasonable bounds */
        y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
        x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
        
        log_trace("Quest vault: Attempting forced placement of '%s' at optimal location (%d,%d) (center: %d,%d)", 
                 v_name + qv_ptr->name, y, x, center_y, center_x);
        
        if (place_room_forced(y, x, qv_ptr)) {
            /* Mark that quest vault was placed in this attempt */
            qv_placed_this_level = true;  /* Track for integrity checks */
            
            /* DEBUGGING: Verify vault actually exists at coordinates immediately after placement */
            int y1 = y - qv_ptr->hgt / 2;
            int x1 = x - qv_ptr->wid / 2;
            int y2 = y1 + qv_ptr->hgt - 1;
            int x2 = x1 + qv_ptr->wid - 1;
            
            int verify_walls = 0, verify_floors = 0, verify_features = 0, verify_monsters = 0;
            int verify_icky = 0, verify_room = 0;
            
            for (int vy = y1; vy <= y2; vy++) {
                for (int vx = x1; vx <= x2; vx++) {
                    if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                        verify_walls++;
                    } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                        verify_floors++;
                    } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                        verify_features++;
                    }
                    
                    if (cave_m_idx[vy][vx] > 0) {
                        verify_monsters++;
                    }
                    
                    if (cave_info[vy][vx] & CAVE_ICKY) {
                        verify_icky++;
                    }
                    
                    if (cave_info[vy][vx] & CAVE_ROOM) {
                        verify_room++;
                    }
                }
            }
            
            log_trace("VAULT VERIFICATION IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)", 
                      y1, x1, y2, x2);
            log_trace("VAULT VERIFICATION: %d walls, %d floors, %d features, %d monsters", 
                      verify_walls, verify_floors, verify_features, verify_monsters);
            log_trace("VAULT VERIFICATION: %d CAVE_ICKY, %d CAVE_ROOM flags", 
                      verify_icky, verify_room);
            
            process_quest_vault_area(y, x, qv_ptr);
            log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using forced strategy", 
                     v_type, v_name + qv_ptr->name, y, x);
            return true;
        } else {
            log_trace("Quest vault: Failed to place vault '%s' at (%d,%d) even with forced strategy", 
                     v_name + qv_ptr->name, y, x);
            /* Try a few more strategic locations before giving up */
            for (int attempts = 0; attempts < 10; attempts++) {
                y = center_y + rand_range(-p_ptr->cur_map_hgt/4, p_ptr->cur_map_hgt/4);
                x = center_x + rand_range(-p_ptr->cur_map_wid/4, p_ptr->cur_map_wid/4);
                y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
                x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));
                
                if (place_room_forced(y, x, qv_ptr)) {
                    /* Mark that quest vault was placed in this attempt */
                    qv_placed_this_level = true;  /* Track for integrity checks */
                    
                    /* DEBUGGING: Verify vault actually exists at coordinates immediately after placement */
                    int y1 = y - qv_ptr->hgt / 2;
                    int x1 = x - qv_ptr->wid / 2;
                    int y2 = y1 + qv_ptr->hgt - 1;
                    int x2 = x1 + qv_ptr->wid - 1;
                    
                    int verify_walls = 0, verify_floors = 0, verify_features = 0, verify_monsters = 0;
                    int verify_icky = 0, verify_room = 0;
                    
                    for (int vy = y1; vy <= y2; vy++) {
                        for (int vx = x1; vx <= x2; vx++) {
                            if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                                verify_walls++;
                            } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                                verify_floors++;
                            } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                                verify_features++;
                            }
                            
                            if (cave_m_idx[vy][vx] > 0) {
                                verify_monsters++;
                            }
                            
                            if (cave_info[vy][vx] & CAVE_ICKY) {
                                verify_icky++;
                            }
                            
                            if (cave_info[vy][vx] & CAVE_ROOM) {
                                verify_room++;
                            }
                        }
                    }
                    
                    log_trace("VAULT VERIFICATION (FALLBACK) IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)", 
                              y1, x1, y2, x2);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d walls, %d floors, %d features, %d monsters", 
                              verify_walls, verify_floors, verify_features, verify_monsters);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d CAVE_ICKY, %d CAVE_ROOM flags", 
                              verify_icky, verify_room);
                    
                    process_quest_vault_area(y, x, qv_ptr);
                    log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using fallback attempt %d", 
                             v_type, v_name + qv_ptr->name, y, x, attempts + 1);
                    return true;
                }
            }
        }
    }
    
    log_trace("Quest vault: No type %d quest vault could be placed even with forced strategy, resetting quest reservation", v_type);
    p_ptr->quest_reserved[0] = 0;
    return false;
}

static void set_perm_boundry(void)
{
    int y, x;

    /* Special boundary walls -- Top */
    for (x = 0; x < p_ptr->cur_map_wid; x++)
    {
        y = 0;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    /* Special boundary walls -- Bottom */
    for (x = 0; x < p_ptr->cur_map_wid; x++)
    {
        y = p_ptr->cur_map_hgt - 1;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    /* Special boundary walls -- Left */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        x = 0;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }

    /* Special boundary walls -- Right */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        x = p_ptr->cur_map_wid - 1;

        /* Clear previous contents, add perma-wall */
        cave_set_feat(y, x, FEAT_WALL_PERM);
    }
}

/* Start new level with a map entirely of basic granite */
static void basic_granite(void)
{
    int y, x;
    int depth_color = get_depth_color(p_ptr->depth);

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Create granite wall with depth-based color */
            cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, depth_color);

            // initialise the corridor id array
            cave_corridor1[y][x] = -1;
            cave_corridor2[y][x] = -1;
        }
    }
}

void make_patch_of_sunlight(int y, int x)
{
    int m, n, floor;

    if (cave_info[y][x] & CAVE_GLOW)
    {
        floor = 0;
        for (n = (y - 1); n <= (y + 1); n++)
        {
            for (m = (x - 1); m <= (x + 1); m++)
            {
                if (cave_feat[n][m] == FEAT_FLOOR)
                    floor++;
            }
        }
        if (floor > 6)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
                cave_set_feat(y, x, FEAT_RUBBLE);
            for (n = (y - 1); n <= (y + 1); n++)
            {
                for (m = (x - 1); m <= (x + 1); m++)
                {
                    if ((cave_info[n][m] & CAVE_GLOW)
                        && cave_feat[n][m] == FEAT_FLOOR && one_in_(4))
                    {
                        if (cave_feat[n][m] == FEAT_FLOOR)
                            cave_set_feat(n, m, FEAT_SUNLIGHT);
                    }
                }
            }
        }
    }
}

void make_patches_of_sunlight()
{
    int i, x, y;

    // bunch near the player
    for (i = 0; i < 40; ++i)
    {
        y = rand_range(MAX(p_ptr->py - 5, 1),
            MIN(p_ptr->py + 5, p_ptr->cur_map_hgt - 2));
        x = rand_range(MAX(p_ptr->px - 5, 1),
            MIN(p_ptr->px + 5, p_ptr->cur_map_wid - 2));
        make_patch_of_sunlight(y, x);
    }

    // and a few scattered over the first level
    for (i = 0; i < 20; ++i)
    {
        y = rand_range(10, p_ptr->cur_map_hgt - 10);
        x = rand_range(10, p_ptr->cur_map_wid - 10);
        make_patch_of_sunlight(y, x);
    }
}

static void ensure_sunlight_for_varda(void)
{
    /* Only relevant for the first few levels */
    if (p_ptr->depth > 3) return;
    
    /* Check for valid sunlight spawn locations (sunlight on floor tiles) */
    bool has_valid_sunlight = false;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1 && !has_valid_sunlight; y++) {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
            if (cave_feat[y][x] == FEAT_SUNLIGHT && 
                cave_floor_bold(y, x) && 
                !(cave_info[y][x] & CAVE_ICKY)) {
                has_valid_sunlight = true;
                break;
            }
        }
    }
    
    if (!has_valid_sunlight) {
        log_trace("Varda spawn: No valid sunlight spawn locations detected, seeding patches");
        make_patches_of_sunlight();
        
        /* Verify at least one valid location exists after patching */
        bool verified = false;
        for (int y = 1; y < p_ptr->cur_map_hgt - 1 && !verified; y++) {
            for (int x = 1; x < p_ptr->cur_map_wid - 1; x++) {
                if (cave_feat[y][x] == FEAT_SUNLIGHT && 
                    cave_floor_bold(y, x) && 
                    !(cave_info[y][x] & CAVE_ICKY)) {
                    verified = true;
                    log_trace("Varda spawn: Verified valid sunlight location at (%d,%d)", y, x);
                    break;
                }
            }
        }
        
        if (!verified) {
            log_trace("Varda spawn: WARNING - No valid sunlight locations after patching!");
        }
    }
}

/*
 * Generate a new dungeon level
 *
 * Note that "dun_body" adds about 4000 bytes of memory to the stack.
 */
static bool cave_gen(void)
{
    int i;

    int l;

    int y, x;

    int room_attempts = 0;

    int is_guaranteed_forge_level = false;
    bool duruin_bastion_forced = false;
    
    /* Reset quest vault monitoring variables for this level */
    qv_placed_this_level = false;
    qv_stored_y1 = qv_stored_x1 = qv_stored_y2 = qv_stored_x2 = -1;
    
    /* Run quest lottery once per level to determine which quest (if any) gets this level */
    run_quest_lottery();
    
    /* Debug: Log entry into cave_gen */
    log_trace("cave_gen: Starting level generation (quest_vault_used=%s, lottery_winner=%d)", 
              p_ptr->quest_vault_used ? "true" : "false", quest_lottery_winner);
    
    /* Varda quest reserves the run to avoid other quest content until complete */
    if (p_ptr->varda_quest >= VARDA_QUEST_ACTIVE && !p_ptr->quest_reserved[0]) {
        p_ptr->quest_reserved[0] = 1;
        log_trace("Varda quest: === QUEST SLOT RESERVED === Active Varda quest reserves slot (state=%d)", p_ptr->varda_quest);
    }
    
    log_trace("cave_gen: Quest status at level start - quest_reserved[0]=%d, varda_quest=%d, lottery_winner=%d",
              p_ptr->quest_reserved[0], p_ptr->varda_quest, quest_lottery_winner);
    
    /* Varda quest: flag forced bastion placement on first level deeper than 500ft */
    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE && !p_ptr->varda_vault_placed && p_ptr->depth > 10) {
        if (!p_ptr->varda_vault_ready) {
            log_trace("Varda quest: Crossing 500ft, setting bastion_ready at depth %d", p_ptr->depth);
        }
        p_ptr->varda_vault_ready = 1;
    }
    s16b mon_gen, obj_room_gen;

    dun_data dun_body;

    /* Global data */
    dun = &dun_body;

    /* Sil - determine the dungeon size */
    /* note: Panel height and width is 1/6 of max height/width*/

    // between 4x4 and 5x5 (cap strictly at 5 to avoid oversize levels)
    l = 4 + ((p_ptr->depth + dieroll(5)) / 10);
    if (l > 5) l = 5;

    p_ptr->cur_map_hgt = l * (PANEL_HGT);
    p_ptr->cur_map_wid = l * (PANEL_WID_FIXED);

    room_attempts = l * l * l * l;
    log_trace("cave_gen: map size set to %dx%d (l=%d) room_attempts=%d", p_ptr->cur_map_wid, p_ptr->cur_map_hgt, l, room_attempts);

    /* Initialize level style weights and start with basic granite */
    styles_init_for_level();
    /*start with basic granite*/
    basic_granite();
    log_trace("cave_gen: after styles_init/basic_granite");

    log_trace("cave_gen: before connection table init (DUN_ROOMS=%d, conn_size=%zu)", DUN_ROOMS, sizeof(dun->connection));
    /* Initialize the connection table */
    for (y = 0; y < DUN_ROOMS; y++)
    {
        if (y == 0 || y == DUN_ROOMS - 1)
            log_trace("cave_gen: init conn row %d start", y);
        for (x = 0; x < DUN_ROOMS; x++)
        {
            dun->connection[y][x] = false;
        }
        log_trace("cave_gen: connection init row=%d done", y);
    }
    log_trace("cave_gen: after connection table init");

    /* No rooms yet */
    dun->cent_n = 0;
    log_trace("cave_gen: cent_n reset to 0");
    layout_anchor_reset();

    /* Verify dun struct sanity */
    log_trace("cave_gen: sanity check dun ptr=%p cent capacity=%d connection[0][0]=%d piece[0]=%d corner[0]=(y1=%d,x1=%d,y2=%d,x2=%d)",
        (void*)dun, DUN_ROOMS, dun->connection[0][0], dun->piece[0],
        dun->corner[0].y1, dun->corner[0].x1, dun->corner[0].y2, dun->corner[0].x2);

    if (cheat_room)
        msg_format("Forge count is %d.", p_ptr->forge_count);

    // guarantee a forge at levels 2, 6, 10 (exactly at those levels, not beyond)
    if (p_ptr->fixed_forge_count < 3)
    {
        int next_guaranteed_forge_level = 2 + (p_ptr->fixed_forge_count * 4);
        is_guaranteed_forge_level = (next_guaranteed_forge_level == p_ptr->depth);
        log_trace("Forge forcing check: fixed_forge_count=%d, target_level=%d, current_depth=%d, forcing=%s", 
                 p_ptr->fixed_forge_count, next_guaranteed_forge_level, p_ptr->depth, 
                 is_guaranteed_forge_level ? "true" : "false");
    }

    if (cheat_room)
        msg_format("Guaranteed forge: %s.",
            is_guaranteed_forge_level ? "true" : "false");

    log_trace("cave_gen: before guaranteed forge handling");
    if (is_guaranteed_forge_level)
    {
        int y = rand_range(5, p_ptr->cur_map_hgt - 5);
        int x = rand_range(5, p_ptr->cur_map_wid - 5);
        log_trace("cave_gen: attempting guaranteed forge at (%d,%d)", y, x);

        if (cheat_room)
            msg_format("Trying to force a forge:");
        p_ptr->force_forge = true;
        p_ptr->fixed_forge_count++;
        log_trace("cave_gen: force_forge=true, fixed_forge_count=%d", p_ptr->fixed_forge_count);

        if (!build_type6(y, x, true))
        {
            if (cheat_room)
                msg_format("failed.");

            p_ptr->fixed_forge_count--;
            return (false);
        }

        if (cheat_room)
            msg_format("succeeded.");
    }
    log_trace("cave_gen: post guaranteed-forge path cent_n=%d", dun->cent_n);
    log_trace("cave_gen: post guaranteed-forge path cent_n=%d", dun->cent_n);

    /* Quest vault determination - Allow re-placement during level regeneration */
    log_trace("Quest vault: ENTERING quest vault logic check (quest_vault_used=%s, force_forge=%s, qv_placed_this_level=%s)", 
              p_ptr->quest_vault_used ? "true" : "false", 
              p_ptr->force_forge ? "true" : "false",
              qv_placed_this_level ? "true" : "false");
    log_trace("Quest vault: Starting quest vault check (quest_vault_used=%s, force_forge=%s)", 
              p_ptr->quest_vault_used ? "true" : "false", 
              p_ptr->force_forge ? "true" : "false");
    
    /* If Varda's quest is active and the bastion is due, force its placement first */
    log_trace("Quest vault check: varda_vault_ready=%d, varda_quest=%d (ACTIVE=%d), varda_vault_placed=%d",
              p_ptr->varda_vault_ready, p_ptr->varda_quest, VARDA_QUEST_ACTIVE, p_ptr->varda_vault_placed);
    
    if (p_ptr->varda_vault_ready && p_ptr->varda_quest == VARDA_QUEST_ACTIVE && !p_ptr->varda_vault_placed) {
        log_trace("Quest vault: === DURUIN BASTION FORCE PLACEMENT === Starting at depth %d", p_ptr->depth);
        if (!place_duruin_bastion()) {
            log_trace("Quest vault: === DURUIN BASTION FAILED === Regenerating level");
            return false;
        }
        log_trace("Quest vault: === DURUIN BASTION SUCCESS === Placed successfully");
        duruin_bastion_forced = true;
    } else if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE) {
        log_trace("Quest vault: Varda quest ACTIVE but bastion not ready (vault_ready=%d, vault_placed=%d)",
                  p_ptr->varda_vault_ready, p_ptr->varda_vault_placed);
    }
              
    /* QUEST VAULT REGENERATION FIX: Allow quest vault re-placement during regeneration */
    /* Quest vaults can be placed if: */
    /* 1. quest_vault_used is false (haven't successfully completed a quest vault this run), OR */
    /* 2. We're in a regeneration scenario (quest vault was placed before but level failed) */
    if (!p_ptr->quest_vault_used && !duruin_bastion_forced)
    {
        /* QUEST VAULT REGENERATION FIX: Remove the quest_vault_attempted_this_level check */
        /* to allow quest vault re-placement during level regeneration */
        
        /* Check if any quest is already active - ONE QUEST PER RUN ENFORCEMENT */
        log_trace("Quest vault: Checking one-quest-per-run enforcement:");
        log_trace("Quest vault:   quest_reserved[0]=%d (should block if 1)", p_ptr->quest_reserved[0]);
        log_trace("Quest vault:   tulkas=%d, mandos=%d, aule=%d, varda=%d",
                  p_ptr->tulkas_quest, p_ptr->mandos_quest, p_ptr->aule_quest, p_ptr->varda_quest);
        
        if (p_ptr->quest_reserved[0] || 
            p_ptr->tulkas_quest != TULKAS_QUEST_NOT_STARTED ||
            p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED ||
            p_ptr->aule_quest != AULE_QUEST_NOT_STARTED ||
            p_ptr->varda_quest != VARDA_QUEST_NOT_STARTED) {
            log_trace("Quest vault: === BLOCKED === Quest already active - one quest per run enforced (tulkas=%d, mandos=%d, aule=%d, varda=%d, reserved=%d)", 
                     p_ptr->tulkas_quest, p_ptr->mandos_quest, p_ptr->aule_quest, p_ptr->varda_quest, p_ptr->quest_reserved[0]);
            /* Don't place any quest vaults - skip to end */
        } else {
            int quest_vault_roll = dieroll(p_ptr->depth + 5);
            log_trace("Quest vault: Level determination roll = %d", quest_vault_roll);

            if (one_in_(5))
            {
                int bonus = dieroll(5);
                quest_vault_roll += bonus;
                log_trace("Quest vault: Bonus roll (+%d) = %d total", bonus, quest_vault_roll);
            }

            bool quest_vault_placed = false;
            
            if (quest_vault_roll >= 18)
            {
                log_trace("Quest vault: Hit greater vault threshold (%d >= 18), trying quest vaults 8->7->6", quest_vault_roll);
                quest_vault_placed = try_quest_vault_type(8) || try_quest_vault_type(7) || try_quest_vault_type(6);
                
                if (!quest_vault_placed) {
                    log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                    return false; /* Force regeneration to guarantee quest vault spawns */
                }
            }
            else if (quest_vault_roll >= 13)
            {
                log_trace("Quest vault: Hit lesser vault threshold (%d >= 13), trying quest vaults 7->6", quest_vault_roll);
                quest_vault_placed = try_quest_vault_type(7) || try_quest_vault_type(6);
                
                if (!quest_vault_placed) {
                    log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                    return false; /* Force regeneration to guarantee quest vault spawns */
                }
            }
            else if (quest_vault_roll >= 8)
            {
                log_trace("Quest vault: Hit interesting room threshold (%d >= 8), trying quest vault 6", quest_vault_roll);
                quest_vault_placed = try_quest_vault_type(6);
                
                if (!quest_vault_placed) {
                    log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                    return false; /* Force regeneration to guarantee quest vault spawns */
                }
            }
            else
            {
                log_trace("Quest vault: Roll too low (%d < 8), no quest vault this level", quest_vault_roll);
            }
            
            if (quest_vault_placed)
            {
                log_trace("Quest vault: Successfully placed quest vault, no more quest vaults this run");
            }
            else
            {
                log_trace("Quest vault: No quest vault placed this level");
            }
        }
    }
    else if (p_ptr->varda_quest >= VARDA_QUEST_ACTIVE)
    {
        log_trace("Quest vault: === VARDA QUEST BLOCKS === No other quest vaults allowed while Varda quest active (state=%d)", p_ptr->varda_quest);
    }
    else if (duruin_bastion_forced)
    {
        log_trace("Quest vault: Bastion already placed for Varda quest, skipping other quest vault attempts this level");
    }
    else
    {
        log_trace("Quest vault: Already used this run, skipping quest vault check (quest_vault_used=1)");
    }

    /* Seed a handful of prefab anchors up front to diversify layout */
    seed_prefab_anchors();

    /* Build some rooms */
    for (i = 0; i < room_attempts; i++)
    {
        int r = dieroll(p_ptr->depth + 5);
        log_trace("Room generation: depth+5 roll = %d", r);

        if (one_in_(5))
        {
            int bonus = dieroll(5);
            r += bonus;
            log_trace("Room generation: bonus roll (+%d) = %d total", bonus, r);
        }

        // choose a room type based on the level
        if ((r < 5) || one_in_(2))
        {
            // standard room
            log_trace("Room generation: Building standard room (r=%d)", r);
            room_build(1);
        }
        else if (r < 8)
        {
            // cross room
            log_trace("Room generation: Building cross room (r=%d)", r);
            room_build(2);
        }
        else if ((r < 13) || one_in_(2))
        {
            // interesting room
            log_trace("Room generation: Building interesting room (r=%d)", r);
            room_build(6);
        }
        else if (r < 18)
        {
            // lesser vault
            log_trace("Room generation: Building lesser vault (r=%d)", r);
            room_build(7);
        }
        else
        {
            // greater vault
            log_trace("Room generation: Building greater vault (r=%d)", r);
            room_build(8);
        }

        // stop if there are too many rooms
        if (dun->cent_n == DUN_ROOMS - 1)
            break;
    }

    /*set the permanent walls*/
    set_perm_boundry();

    /* Carve CA blob anchors into remaining granite */
    seed_ca_blob_anchors();
    /* Add BSP-slice anchors for rectangular-but-offset caverns */
    seed_bsp_slice_anchors();

    layout_anchor_capture_existing_rooms();

    /* Log final room count for debugging */
    log_trace("Room generation completed: %d rooms generated (quest_vault_placed=%s)", 
              dun->cent_n, qv_placed_this_level ? "true" : "false");

    /*start over on all levels with less than two rooms due to inevitable
     * crash*/
    /* QUEST VAULT FIX: Use original room requirement, quest vault regeneration will be handled differently */
    if (dun->cent_n < ROOM_MIN)
    {
        if (cheat_room)
            msg_format("Not enough rooms (%d < %d).", dun->cent_n, ROOM_MIN);
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: Only %d rooms generated, minimum %d required", dun->cent_n, ROOM_MIN);
        return (false);
    }

    /* DEBUGGING: Check if quest vault still exists after room generation */
    check_quest_vault_integrity("AFTER_ROOM_GENERATION");

    /* make the tunnels */
    /* Sil - This has been changed considerably */
    if (!connect_rooms_stairs())
    {
        if (cheat_room)
            msg_format("Couldn't connect the rooms.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: connect_rooms_stairs() returned false");
        return (false);
    }

    /* DEBUGGING: Check if quest vault still exists after tunnel making */
    check_quest_vault_integrity("AFTER_TUNNEL_GENERATION");

    /* randomise the doors (except those in vaults) */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if ((cave_feat[y][x] == FEAT_DOOR_HEAD)
                && !(cave_info[y][x] & (CAVE_ICKY)))
            {
                if (one_in_(4))
                    cave_set_feat(y, x, FEAT_FLOOR);
                else
                    place_random_door(y, x);
            }
        }
    squash_double_doors();

    /* DEBUGGING: Check if quest vault still exists after door randomization */
    check_quest_vault_integrity("AFTER_DOOR_RANDOMIZATION");

    /* place the stairs, traps, rubble, secret doors, and player */
    if (!place_rubble_player())
    {
        if (cheat_room)
            msg_format("Couldn't place, rubble, or player.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: place_rubble_player() returned false");
        return (false);
    }

    if (p_ptr->depth == 1)
    {
        // smaller number of monsters at 50ft
        mon_gen = dun->cent_n / 2;
        // game start
        if (p_ptr->stairs_taken == 0)
            make_patches_of_sunlight();
    }
    else
    {
        // pick some number of monsters (between 0.5 per room and 1 per room)
        mon_gen = (dun->cent_n + dieroll(dun->cent_n)) / 2;
    }

        /* meta-run curse: more monsters */
    {
        int stacks = curse_flag_count_cur(CUR_MON_NUM);
        if (stacks)
            mon_gen = mon_gen * (100 + 30 * stacks) / 100; /* +30 % each */
    }

    // check dungeon connectivity
    if (!check_connectivity())
    {
        if (cheat_room)
            msg_format("Failed connectivity.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: check_connectivity() returned false");
        return (false);
    }

    /* Put some objects in rooms */
    obj_room_gen = 3 * mon_gen / 4;
    if (obj_room_gen > 0)
    {
        // currently ignoring the above...
        alloc_object(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, obj_room_gen, false);
    }

    // place the traps
    place_traps();

    /* Put some monsters in the dungeon */
    for (i = mon_gen; i > 0; i--)
    {
        (void)alloc_monster(false, false);
    }
    
    /* Check for Varda quest spawning - lottery-based */
    log_trace("Varda spawn check: lottery_winner=%d (QUEST_ID_VARDA=%d), depth=%d, varda_quest=%d", 
              quest_lottery_winner, QUEST_ID_VARDA, p_ptr->depth, p_ptr->varda_quest);
    
    if (quest_lottery_winner == QUEST_ID_VARDA) { /* Varda is quest ID 6 */
        log_trace("Varda spawn: === VARDA WON LOTTERY === Attempting spawn at depth %d", p_ptr->depth);
        log_trace("Varda spawn: Current state - varda_quest=%d, quest_reserved[0]=%d", 
                  p_ptr->varda_quest, p_ptr->quest_reserved[0]);
        
        /* Safety: enforce early-depth requirement even if data is misconfigured */
        if (p_ptr->depth > 3) {
            log_trace("Varda spawn: FAILED - depth %d exceeds allowed range 1-3", p_ptr->depth);
            return false; /* Force regeneration until early depth is honored */
        }
        
        /* Ensure there is at least some sunlight on the level */
        log_trace("Varda spawn: Ensuring sunlight exists on level");
        ensure_sunlight_for_varda();
        log_trace("Varda spawn: Sunlight check complete");
        
        /* Check if Varda already exists on this level */
        log_trace("Varda spawn: Checking if Varda already exists on this level (mon_max=%d)", mon_max);
        bool varda_exists = false;
        for (int j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_VARDA)
            {
                varda_exists = true;
                log_trace("Varda spawn: Found existing Varda at monster index %d", j);
                break;
            }
        }
        
        if (!varda_exists)
        {
            log_trace("Varda spawn: No existing Varda found, attempting placement");
            bool varda_spawned = false;
            int valid_attempts = 0;
            int invalid_no_sunlight = 0;
            int invalid_not_floor = 0;
            int invalid_has_monster = 0;
            int invalid_icky = 0;
            
            /* Prefer to spawn near the player start on a sunlit tile */
            log_trace("Varda spawn: Starting placement attempts (max 120)");
            for (int attempts = 0; attempts < 120 && !varda_spawned; attempts++)
            {
                int try_y = rand_int(p_ptr->cur_map_hgt);
                int try_x = rand_int(p_ptr->cur_map_wid);
                
                if (try_y <= 0 || try_y >= p_ptr->cur_map_hgt - 1 ||
                    try_x <= 0 || try_x >= p_ptr->cur_map_wid - 1)
                    continue;
                
                if (cave_feat[try_y][try_x] != FEAT_SUNLIGHT) { invalid_no_sunlight++; continue; }
                if (!cave_floor_bold(try_y, try_x)) { invalid_not_floor++; continue; }
                if (cave_m_idx[try_y][try_x] != 0) { invalid_has_monster++; continue; }
                if (cave_info[try_y][try_x] & CAVE_ICKY) { invalid_icky++; continue; }
                
                valid_attempts++;
                
                if (place_monster_one(try_y, try_x, R_IDX_VARDA, true, true, NULL))
                {
                    p_ptr->varda_quest = VARDA_QUEST_GIVER_PRESENT;
                    p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                    p_ptr->varda_level = p_ptr->depth;
                    varda_spawned = true;
                    log_trace("Varda spawn: === SUCCESS === Placed at (%d,%d) on sunlight tile", try_y, try_x);
                    log_trace("Varda spawn: Quest state set to GIVER_PRESENT (%d), quest_reserved[0]=1", p_ptr->varda_quest);
                }
            }
            
            log_trace("Varda spawn: Placement attempts complete - valid=%d, no_sunlight=%d, not_floor=%d, has_monster=%d, icky=%d",
                      valid_attempts, invalid_no_sunlight, invalid_not_floor, invalid_has_monster, invalid_icky);
            
            if (!varda_spawned)
            {
                log_trace("Varda spawn: === FAILED === Could not find valid sunlight tile after 120 attempts - REGENERATING LEVEL");
                return false; /* Force regeneration to honor 100% spawn guarantee */
            }
        }
        else
        {
            log_trace("Varda spawn: Varda already present on level, skipping placement");
        }
    } else {
        log_trace("Varda spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    /* Check for Tulkas quest spawning - only if it won the lottery */
    int tulkas_completions = metarun_quest_completion_count(METARUN_QUEST_TULKAS);
    log_trace("Tulkas spawn check: quest=%d, depth=%d, metarun_completions=%d, lottery_winner=%d", 
             p_ptr->tulkas_quest, p_ptr->depth, tulkas_completions, quest_lottery_winner);
             
    /* Only attempt Tulkas spawning if it won the lottery */
    if (quest_lottery_winner == 1) { /* Tulkas is quest ID 1 */
        log_trace("Tulkas spawn: Tulkas WON the lottery - attempting spawn");
        
        /* Try to find a room to spawn Tulkas in */
        int attempts;
        bool tulkas_spawned = false;
        
        log_trace("Tulkas spawn: Lottery winner attempting placement at depth %d", p_ptr->depth);
        
        /* Check if Tulkas already exists on this level */
        bool tulkas_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_TULKAS)
                {
                    tulkas_exists = true;
                    break;
                }
            }
            
            if (!tulkas_exists)
            {
                /* Try to spawn Tulkas near the player's starting room */
                int player_y = p_ptr->py;
                int player_x = p_ptr->px;
                
                /* Try to find a spot in the same room as the player first */
                for (attempts = 0; attempts < 50 && !tulkas_spawned; attempts++)
                {
                    /* Search in a radius around the player */
                    int dy = rand_range(-2, 2);
                    int dx = rand_range(-2, 2);
                    int try_y = player_y + dy;
                    int try_x = player_x + dx;
                    
                    /* Must be valid coordinates and a floor in the same room */
                    if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                        try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(try_y, try_x) && 
                        (cave_info[try_y][try_x] & CAVE_ROOM) &&
                        !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                        cave_m_idx[try_y][try_x] == 0)
                    {
                        if (place_monster_one(try_y, try_x, R_IDX_TULKAS, true, true, NULL))
                        {
                            p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                            tulkas_spawned = true;
                            log_trace("Tulkas spawned near player at (%d, %d), player at (%d, %d), quest state: %d", 
                                     try_y, try_x, player_y, player_x, p_ptr->tulkas_quest);
                        }
                    }
                }
                
                /* If that failed, try any room on the level */
                if (!tulkas_spawned)
                {
                    for (attempts = 0; attempts < 100 && !tulkas_spawned; attempts++)
                    {
                        int room_y = rand_int(p_ptr->cur_map_hgt);
                        int room_x = rand_int(p_ptr->cur_map_wid);
                        
                        /* Must be a floor in a room, not in a vault/interesting room */
                        if (cave_floor_bold(room_y, room_x) && 
                            (cave_info[room_y][room_x] & CAVE_ROOM) &&
                            !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                            cave_m_idx[room_y][room_x] == 0)
                        {
                            if (place_monster_one(room_y, room_x, R_IDX_TULKAS, true, true, NULL))
                            {
                                p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
                                p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                                tulkas_spawned = true;
                                log_trace("Tulkas spawned in fallback room at (%d, %d), quest state: %d", 
                                         room_y, room_x, p_ptr->tulkas_quest);
                            }
                        }
                    }
                }
                
                if (!tulkas_spawned)
                {
                    log_trace("Failed to spawn Tulkas in room after all attempts");
                }
            }
            else
            {
                log_trace("Tulkas already exists on level, skipping room spawn");
            }
    }

    /* Check for Niena room-based spawning - LOTTERY SYSTEM */
    int niena_completions = metarun_quest_completion_count(METARUN_QUEST_NIENA);
    log_trace("Niena spawn check: quest=%d, depth=%d, level_size_l=%d, metarun_completions=%d, lottery_winner=%d", 
             p_ptr->niena_quest, p_ptr->depth, l, niena_completions, quest_lottery_winner);
             
    /* Only attempt Niena spawning if it won the lottery */
    if (quest_lottery_winner == 4) { /* Niena is quest ID 4 */
        log_trace("Niena spawn: Niena WON the lottery - attempting spawn");
        
        /* Check level size requirement: must be maximum size (l >= 5) */
        if (l < 5) {
            log_trace("Niena spawn: FAILED - level too small (l=%d, need l>=5)", l);
            return false; /* Force regeneration until we get a big enough level */
        }
        
        /* Check stair distance requirement: must be at least 87 grid distance */
        int min_stair_dist = calculate_min_stair_distance();
        log_trace("Niena spawn: Calculated minimum stair distance = %d", min_stair_dist);
        
        if (min_stair_dist < 87) {
            log_trace("Niena spawn: FAILED - stairs too close (distance=%d, need >=87)", min_stair_dist);
            return false; /* Force regeneration until stairs are far enough apart */
        }
        
        log_trace("Niena spawn: Stair distance check PASSED (distance=%d >= 87)", min_stair_dist);
        
        /* Try to find a room to spawn Niena in near the up stairs */
        int attempts;
        bool niena_spawned = false;
        
        log_trace("Niena spawn: Lottery winner attempting placement at depth %d, level_size=%d, stair_distance=%d", 
                  p_ptr->depth, l, min_stair_dist);
        
        /* Check if Niena already exists on this level */
        bool niena_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_NIENA)
            {
                niena_exists = true;
                break;
            }
        }
        
        if (!niena_exists)
        {
            /* Try to spawn Niena near the player's starting position (up stairs) */
            int player_y = p_ptr->py;
            int player_x = p_ptr->px;
            
            log_trace("Niena spawn: Attempting to place near player at (%d,%d)", player_y, player_x);
            
            /* Verify player has valid coordinates */
            if (player_y > 0 && player_y < p_ptr->cur_map_hgt - 1 &&
                player_x > 0 && player_x < p_ptr->cur_map_wid - 1)
            {
                /* Try to find a spot in the same room as the player first */
                for (attempts = 0; attempts < 50 && !niena_spawned; attempts++)
                {
                    /* Search in a radius around the player */
                    int dy = rand_range(-2, 2);
                    int dx = rand_range(-2, 2);
                    int try_y = player_y + dy;
                    int try_x = player_x + dx;
                    
                    /* Must be valid coordinates and a floor in the same room */
                    if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                        try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(try_y, try_x) && 
                        (cave_info[try_y][try_x] & CAVE_ROOM) &&
                        !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                        cave_m_idx[try_y][try_x] == 0)
                    {
                        if (place_monster_one(try_y, try_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                            niena_spawned = true;
                            log_trace("Niena spawned near player at (%d, %d), player at (%d, %d), quest state: %d", 
                                     try_y, try_x, player_y, player_x, p_ptr->niena_quest);
                        }
                    }
                }
            }
            else
            {
                log_trace("Niena spawn: Invalid player coordinates (%d,%d), falling back to any room", player_y, player_x);
            }
            
            /* If that failed, try any room on the level */
            if (!niena_spawned)
            {
                log_trace("Niena spawn: Near-player placement failed, trying any suitable room");
                for (attempts = 0; attempts < 100 && !niena_spawned; attempts++)
                {
                    int room_y = rand_int(p_ptr->cur_map_hgt);
                    int room_x = rand_int(p_ptr->cur_map_wid);
                    
                    /* Must be valid coordinates and a floor in a room */
                    if (room_y > 0 && room_y < p_ptr->cur_map_hgt - 1 &&
                        room_x > 0 && room_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(room_y, room_x) && 
                        (cave_info[room_y][room_x] & CAVE_ROOM) &&
                        !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                        cave_m_idx[room_y][room_x] == 0)
                    {
                        if (place_monster_one(room_y, room_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                            niena_spawned = true;
                            log_trace("Niena spawned in fallback room at (%d, %d), quest state: %d", 
                                     room_y, room_x, p_ptr->niena_quest);
                        }
                    }
                }
            }
            
            if (!niena_spawned)
            {
                log_trace("Niena spawn: FAILED to spawn after all attempts - forcing regeneration");
                return false; /* Force regeneration */
            }
        }
        else
        {
            log_trace("Niena already exists on level, skipping room spawn");
        }
    } else {
        log_trace("Niena spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    /* Check for Orom├½ quest spawning - only if it won the lottery */
    int orome_completions = metarun_quest_completion_count(METARUN_QUEST_OROME);
    bool orome_blocked = quest_metarun_blocked(QUEST_ID_OROME, METARUN_QUEST_OROME);
    log_trace("Orom├½ spawn check: quest=%d, depth=%d, metarun_completions=%d, lottery_winner=%d, blocked=%s", 
             p_ptr->orome_quest, p_ptr->depth, 
             orome_completions,
             quest_lottery_winner,
             orome_blocked ? "yes" : "no");
             
    /* Only attempt Orom├½ spawning if it won the lottery and isn't blocked by metarun history */
    if (orome_blocked) {
        log_trace("Orom├½ spawn: blocked by metarun state (requires active oath or under cap)");
        quest_lottery_winner = 0; /* Treat level as quest-free if history blocks this quest */
    } else if (quest_lottery_winner == 5) { /* Orom├½ is quest ID 5 */
        log_trace("Orom├½ spawn: Orom├½ WON the lottery - attempting spawn");
        
        /* Try to find a room to spawn Orom├½ in */
        int attempts;
        bool orome_spawned = false;
        
        log_trace("Orom├½ spawn: Lottery winner attempting placement at depth %d", p_ptr->depth);
        
        /* Check if Orom├½ already exists on this level */
        bool orome_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_OROME)
            {
                orome_exists = true;
                break;
            }
        }
        
        if (!orome_exists)
        {
            /* Try to spawn Orom├½ near the player's starting room */
            int player_y = p_ptr->py;
            int player_x = p_ptr->px;
            
            /* Try to find a spot in the same room as the player first */
            for (attempts = 0; attempts < 50 && !orome_spawned; attempts++)
            {
                /* Search in a radius around the player */
                int dy = rand_range(-2, 2);
                int dx = rand_range(-2, 2);
                int try_y = player_y + dy;
                int try_x = player_x + dx;
                
                /* Must be valid coordinates and a floor in the same room */
                if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                    try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                    cave_floor_bold(try_y, try_x) && 
                    (cave_info[try_y][try_x] & CAVE_ROOM) &&
                    !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                    cave_m_idx[try_y][try_x] == 0)
                {
                    if (place_monster_one(try_y, try_x, R_IDX_OROME, true, true, NULL))
                    {
                        p_ptr->orome_quest = OROME_QUEST_GIVER_PRESENT;
                        p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                        orome_spawned = true;
                        log_trace("Orom├½ spawned near player at (%d, %d), player at (%d, %d), quest state: %d", 
                                 try_y, try_x, player_y, player_x, p_ptr->orome_quest);
                    }
                }
            }
            
            /* If that failed, try any room on the level */
            if (!orome_spawned)
            {
                for (attempts = 0; attempts < 100 && !orome_spawned; attempts++)
                {
                    int room_y = rand_int(p_ptr->cur_map_hgt);
                    int room_x = rand_int(p_ptr->cur_map_wid);
                    
                    /* Must be a floor in a room, not in a vault/interesting room */
                    if (cave_floor_bold(room_y, room_x) && 
                        (cave_info[room_y][room_x] & CAVE_ROOM) &&
                        !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                        cave_m_idx[room_y][room_x] == 0)
                    {
                        if (place_monster_one(room_y, room_x, R_IDX_OROME, true, true, NULL))
                        {
                            p_ptr->orome_quest = OROME_QUEST_GIVER_PRESENT;
                            p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                            orome_spawned = true;
                            log_trace("Orom├½ spawned in fallback room at (%d, %d), quest state: %d", 
                                     room_y, room_x, p_ptr->orome_quest);
                        }
                    }
                }
            }
            
            if (!orome_spawned)
            {
                log_trace("Orom├½ spawn: FAILED - could not place monster after 150 attempts");
                return false; /* Force regeneration */
            }
        }
        else
        {
            log_trace("Orom├½ already exists on level, skipping room spawn");
        }
    } else {
        log_trace("Orom├½ spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    // place Morgoth if on the run
    if (p_ptr->on_the_run && !p_ptr->morgoth_slain)
    {
        /* simple way to place Morgoth */
        for (i = 0; i <= 100; i++)
        {
            int danger_factor = 6 - silmarils_possessed();

            y = rand_int(p_ptr->cur_map_hgt);
            x = rand_int(p_ptr->cur_map_wid);

            // pull Morgoth's start toward the player more based on the
            // silmarils the player has
            if (p_ptr->px < x)
                x -= 2 * ((x - p_ptr->px) / danger_factor);
            if (p_ptr->px > x)
                x += 2 * ((p_ptr->px - x) / danger_factor);
            if (p_ptr->py < y)
                y -= 2 * ((y - p_ptr->py) / danger_factor);
            if (p_ptr->py > y)
                y += 2 * ((p_ptr->py - y) / danger_factor);

            if (cave_naked_bold(y, x) && !los(p_ptr->py, p_ptr->px, y, x)
                && !(cave_info[y][x] & (CAVE_ICKY)))
            {
                place_monster_one(y, x, R_IDX_MORGOTH, false, true, NULL);
                break;
            }
        }
    }

    p_ptr->force_forge = false;

    return (true);
}

/*
 * Create the gates to Angband level
 */
static void gates_gen(void)
{
    int y, x;
    int i;
    int py = 0, px = 0;

    /* Restrict to single-screen size */
    p_ptr->cur_map_hgt = (3 * PANEL_HGT);
    p_ptr->cur_map_wid = (2 * PANEL_WID_FIXED);

    /* Initialize level style weights for depth 0 */
    styles_init_for_level();
    /* If no primary style was selected (e.g., no rules loaded yet), force style 13 */
    if (styles_get_level_primary_style() < 0) {
        styles_set_loaded_level_primary(13);
        log_info("gates_gen: forced level primary style to 13 for depth 0");
    }

    /*start with basic granite*/
    basic_granite();

    /*set the permanent walls*/
    set_perm_boundry();

    build_type10(17, 33);

    /* Find an up staircase */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_feat[y][x] == FEAT_MORE)
            {
                py = y;
                px = x;
            }
        }
    }

    if ((py == 0) || (px == 0))
    {
        msg_format("Failed to find a down staircase in the gates level");
    }

    /* Delete any monster on the starting square */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only get the monster on the same square */
        if ((m_ptr->fy != py) || (m_ptr->fx != px))
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }

    /* Place the player */
    player_place(py, px);
}

/*
 * Create the level containing Morgoth's throne room
 */
static void throne_gen(void)
{
    int y, x;
    int i;
    int py = 0, px = 0;

    // display the throne poetry
    pause_with_text(throne_poetry, 5, 13, NULL, 0);

    // set the 'truce' in action
    p_ptr->truce = true;

    /* Restrict to single-screen size */
    p_ptr->cur_map_hgt = (3 * PANEL_HGT);
    p_ptr->cur_map_wid = (3 * PANEL_WID_FIXED);

    /*start with basic granite*/
    basic_granite();

    /*set the permanent walls*/
    set_perm_boundry();

    build_type9(16, 38);

    /* Find an up staircase */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            // Sil-y: assumes the important staircase is at the centre of the
            // level
            if ((cave_feat[y][x] == FEAT_LESS) && (x >= 30) && (x <= 45))
            {
                py = y;
                px = x;
            }
        }
    }

    if ((py == 0) || (px == 0))
    {
        msg_format("Failed to find an up staircase in the throne-room");
    }

    /* Delete any monster on the starting square */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only get the monster on the same square */
        if ((m_ptr->fy != py) || (m_ptr->fx != px))
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }

    /* Place the player */
    player_place(py, px);
}

/*
 * Dungeon generation can set some flags indicating that certain one-off
 * things have happened (artefacts, unique greater vaults, unique forge).
 * But if generation fails, we need to reset these flags.
 *
 * "You can't unring a bell." -- Tom Waits
 */
void unring_a_bell(void)
{
    object_type* o_ptr;
    int y, x, i;

    // look through the dungeon objects for artefacts
    for (i = 1; i < o_max; i++)
    {
        /* Get the object */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];
            a_ptr->cur_num = 0;
        }
    }

    // Look through the map for the unique forge
    for (y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            // Reset the unique forge
            if ((cave_feat[y][x] >= FEAT_FORGE_UNIQUE_HEAD)
                && (cave_feat[y][x] <= FEAT_FORGE_UNIQUE_TAIL))
            {
                p_ptr->unique_forge_made = false;
            }
        }
    }

    /* DEBUGGING: Final check if quest vault still exists at end of generation */
    check_quest_vault_integrity("END_OF_GENERATION");

    // If there is a greater vault...
    if (g_vault_name[0] != '\0')
    {
        // wipe vault name
        g_vault_name[0] = '\0';

        // look for the final greater vault entry
        for (i = 0; i < MAX_GREATER_VAULTS; i++)
        {
            // wipe the final entry
            if (i == MAX_GREATER_VAULTS - 1)
            {
                p_ptr->greater_vaults[i] = 0;
                break;
            }
            else if (p_ptr->greater_vaults[i + 1] == 0)
            {
                p_ptr->greater_vaults[i] = 0;
                break;
            }
        }
    }
}

/*
 * Generate a random dungeon level
 *
 * Hack -- regenerate any "overflow" levels
 *
 * Note that this function resets "cave_feat" and "cave_info" directly.
 */
void generate_cave(void)
{
    int y, x, i;

    log_info("generate_cave: Function entry - about to start");
    log_debug("generate_cave: Starting cave generation");

    /* Reset per-level color cache so depth group re-rolls when entering a new level */
    reset_depth_color_cache();

    /* The dungeon is not ready */
    character_dungeon = false;

    /* Don't know feeling yet */
    do_feeling = 0;

    /*allow uniques to be generated everywhere but in nests/pits*/
    allow_uniques = true;

    /* Restrict quest monsters from spawning outside their quest contexts */
    get_mon_num_hook = quest_monster_spawn_okay;

    // display the entry poetry
if (playerturn == 0) {
    char extra[4][100];
    int idx = 0;

    /* Prepare pointers */
    const char *name = c_name + current_character_profile->name;
    const char *alt = c_name + current_character_profile->alt_name;
    const char *start = c_name + current_character_profile->start_string;

    /* Line 1: CharacterName AltName! */
    strnfmt(extra[idx], 100, "%s%s!", name, alt);
    idx++;

    /* Split start string (motto) at first '-' */
    const char *dash_start = strchr(start, '-');
    if (dash_start) {
        /* Line 2: up to and including dash */
        strnfmt(extra[idx], 100, "%.*s",
                (int)(dash_start - start + 1), start);
        idx++;
        /* Line 3: remainder after dash */
        strnfmt(extra[idx], 100, "%s", dash_start + 1);
        idx++;
    } else {
        /* No dash: all in one line */
        strnfmt(extra[idx], 100, "%s", start);
        idx++;
    }

    /* sentinel */
    extra[idx][0] = '\0';

    /* display banner + stanza */
    pause_with_text(entry_poetry, 4, 13, extra, TERM_YELLOW);
}


    /* Safety check: make sure cave_color is allocated */
    if (!cave_color) {
        log_error("generate_cave: cave_color array is not allocated!");
        return;
    }

    // reset smithing leftover (as there is no access to the old forge)
    p_ptr->smithing_leftover = 0;

    // reset the forced skipping of next turn (a bit rough to miss first turn if
    // you fell down)
    p_ptr->skip_next_turn = false;

    while (true)
    {
        bool okay = true;
        bool quest_vault_placed_this_attempt = false; /* Track if quest vault placed in this attempt */

        cptr why = NULL;
        
        /* QUEST VAULT REGENERATION DEBUG: Log each regeneration attempt */
        log_trace("QUEST VAULT FIX: Starting level generation attempt (quest_vault_used=%s)",
                  p_ptr->quest_vault_used ? "true" : "false");

        /* Reset pending quest state changes at the start of each generation attempt */
        reset_pending_quest_states();
        
        /* Reset quest states that may have been set during previous failed attempts */
        reset_quest_vault_states();

        /* Paranoia: Check that cave_color is allocated */
        if (!cave_color)
        {
            log_error("cave_color array is not allocated!");
            quit("cave_color array not allocated");
        }

        /* Reset */
        o_max = 1;
        mon_max = 1;
        feeling = 0;

        /* Start with a blank cave */
        for (y = 0; y < MAX_DUNGEON_HGT; y++)
        {
            for (x = 0; x < MAX_DUNGEON_WID; x++)
            {
                /* No flags */
                cave_info[y][x] = 0;

                /* No features */
                cave_feat[y][x] = 0;

                /* No colors (use default) */
                cave_color[y][x] = 0;

                /* No objects */
                cave_o_idx[y][x] = 0;

                /* No monsters */
                cave_m_idx[y][x] = 0;

                for (i = 0; i < MAX_FLOWS; i++)
                {
                    cave_cost[i][y][x] = FLOW_MAX_DIST;
                }

                cave_when[y][x] = 0;
            }
        }

    log_debug("generate_cave: Cave initialization completed successfully");

        // reset the wandering monster pauses
        for (i = 0; i < MAX_FLOWS; i++)
        {
            wandering_pause[i] = 0;
        }

        /* Mega-Hack -- no player yet */
        p_ptr->px = p_ptr->py = 0;

        /* Hack -- illegal panel */
        p_ptr->wy = MAX_DUNGEON_HGT;
        p_ptr->wx = MAX_DUNGEON_WID;

        /* Reset the monster generation level */
        monster_level = p_ptr->depth;

        /* Reset the object generation level */
        object_level = p_ptr->depth;

        /* Nothing special here yet */
        good_item_flag = false;

        /* Nothing good here yet */
        rating = 0;

        /* Build the gates to Angband */
        if (!p_ptr->depth)
        {
            gates_gen();

            /* Hack -- Clear stairs request */
            p_ptr->create_stair = 0;
        }

        /* Build Morgoth's throne room */
        else if (p_ptr->depth == MORGOTH_DEPTH)
        {
            throne_gen();

            /* Hack -- Clear stairs request */
            p_ptr->create_stair = 0;
        }

        /* Build a real level */
        else
        {
            /* Make a dungeon, or report the failure to make one*/
            if (cave_gen())
            {
                okay = true;
                /* Check if quest vault was placed during this level generation */
                if (qv_placed_this_level) {
                    quest_vault_placed_this_attempt = true;
                }
                /* Also check if we have pending quest state changes that indicate a quest vault was placed */
                if (pending_quest_states.has_aule_change || pending_quest_states.has_mandos_change || pending_quest_states.has_varda_change) {
                    quest_vault_placed_this_attempt = true;
                }
            }
            else
            {
                okay = false;
            }
        }

        /*message*/
        if (!okay)
        {
            if (cheat_room || cheat_hear || cheat_peek || cheat_xtra)
                why = "defective level";

            // Must reset all the artefacts that were generated on the defective
            // level
            for (i = 1; i < o_max; i++)
            {
                /* Get the object */
                object_type* o_ptr = &o_list[i];

                /* Skip dead objects */
                if (!o_ptr->k_idx)
                    continue;

                /* If artefact. */
                if (o_ptr->name1)
                {
                    /* Reset its count */
                    a_info[o_ptr->name1].cur_num = 0;
                    a_info[o_ptr->name1].found_num = 0;
                }
            }
        }

        else
        {
            /* Extract the feeling */
            if (!feeling)
            {
                if (rating > 100)
                    feeling = 2;
                else if (rating > 80)
                    feeling = 3;
                else if (rating > 60)
                    feeling = 4;
                else if (rating > 40)
                    feeling = 5;
                else if (rating > 30)
                    feeling = 6;
                else if (rating > 20)
                    feeling = 7;
                else if (rating > 10)
                    feeling = 8;
                else if (rating > 0)
                    feeling = 9;
                else
                    feeling = 10;

                /* Hack -- Have a special feeling sometimes */
                if (good_item_flag && !(PRESERVE_MODE))
                    feeling = 1;

                /* Hack -- no feeling at the gates */
                if (!p_ptr->depth)
                    feeling = 0;
            }

            /* Prevent object over-flow */
            if (o_max >= z_info->o_max)
            {
                /* Message */
                why = "too many objects";

                /* Message */
                okay = false;
            }

            /* Prevent monster over-flow */
            if (mon_max >= MAX_MONSTERS)
            {
                /* Message */
                why = "too many monsters";

                /* Message */
                okay = false;
            }
        }

        /* Accept */
        if (okay)
        {
            /* QUEST VAULT REGENERATION FIX: Apply pending quest state changes when level generation is COMPLETELY successful */
            apply_pending_quest_states();
            
            /* QUEST VAULT REGENERATION FIX: Only mark quest_vault_used when level generation is COMPLETELY successful */
            /* This ensures quest vaults can be re-placed during regeneration attempts */
            if (quest_vault_placed_this_attempt) {
                p_ptr->quest_vault_used = 1;
                log_trace("QUEST VAULT FIX: Level completely successful - setting quest_vault_used = 1");
            } else {
                log_trace("QUEST VAULT FIX: Level successful but no quest vault placed this attempt");
            }
            log_trace("QUEST VAULT FIX: Breaking from regeneration loop with successful level");
            break;
        }

        /* Message */
        if (why)
        {
            msg_format("Generation failed (%s)", why);
            log_trace("QUEST VAULT FIX: Level generation failed (%s), regenerating (quest_vault_used=%s)",
                      why, p_ptr->quest_vault_used ? "true" : "false");
        }
        else
        {
            log_trace("QUEST VAULT FIX: Level generation failed (unknown reason), regenerating (quest_vault_used=%s)",
                      p_ptr->quest_vault_used ? "true" : "false");
        }

        // Undo unique things!
        unring_a_bell();

        /* Wipe the objects */
        wipe_o_list();

        /* Wipe the monsters */
        wipe_mon_list();
    }

    /* The dungeon is ready */
    character_dungeon = true;

    /* Reset the number of traps on the level. */
    num_trap_on_level = 0;

    /* Note any forges generated -- have to do this here in case generation
     * fails earlier */
    for (y = 0; y < MAX_DUNGEON_HGT; y++)
    {
        for (x = 0; x < MAX_DUNGEON_WID; x++)
        {
            if (cave_forge_bold(y, x))
            {
                p_ptr->forge_count++;
            }
        }
    }

    // Valar quest doesn't provide map rewards like the old thrall quest
}

