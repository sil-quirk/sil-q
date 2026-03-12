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
#include "gen-log.h"
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
    {'K', "4da7998251196a35", 0, false, true, true}, /* Ancalagon the Black */
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
        log_trace("Quest lottery: quest %d (Tulkas) WINS! (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
            quest_id, dice_roll, tulkas_chance, 100.0f / tulkas_chance);
    } else {
        log_trace("Quest lottery: quest %d (Tulkas) roll failed (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
            quest_id, dice_roll, tulkas_chance, 100.0f / tulkas_chance);
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
            log_trace("Quest lottery: quest %d (Niena) WINS! (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
                quest_id, dice_roll, niena_chance, niena_probability * 100.0f);
        } else {
            log_trace("Quest lottery: quest %d (Niena) roll failed (rolled %d, needed 0, chance was 1/%d = %.1f%%)",
                quest_id, dice_roll, niena_chance, niena_probability * 100.0f);
        }
        
        return won;
    } else {
        log_trace("Quest lottery: quest %d (Niena) probability is 0%% at depth %d", quest_id, depth);
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
#define CENT_MAX DUN_ROOMS  /* Keep room storage in lockstep with connection matrix */
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
static dun_data dun_body;
static dun_data* dun = &dun_body;

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

/* Shared guard to prevent room/anchor writes past allocated arrays/connection matrix */
static inline int room_capacity_limit(void)
{
    /* Reserve the last slot as earlier code did with DUN_ROOMS - 1 */
    int cap = MIN(CENT_MAX, DUN_ROOMS - 1);
    return cap;
}

typedef enum quadrant_mode {
    QUAD_MODE_ROOMY = 0,
    QUAD_MODE_CAVEY,
    QUAD_MODE_RUINED,
    QUAD_MODE_LABYRINTH,  /* Twisting corridors and small chambers */
    QUAD_MODE_CHASM,      /* Large open areas with pillars and bridges */
    QUAD_MODE_BIG_CAVE    /* Single large cavern filling most of the partition */
} quadrant_mode_t;

typedef enum density_level {
    DENSITY_SPARSE = 0,   /* Fewer rooms/carvings */
    DENSITY_NORMAL,       /* Standard amount */
    DENSITY_DENSE         /* More rooms/carvings */
} density_level_t;

#define LABYRINTH_START_DEPTH 7
#define BIG_CAVE_START_DEPTH 10
#define CHASM_START_DEPTH 14
#define SPECIAL_CAP_STEP 5
#define SPECIAL_CAP_MAX 3
/* Special-mode depth gates and caps (tweak to rebalance rarity) */

/* Persist the chosen partition grid for later passes (connectivity, stair guarantees) */
static int current_partition_rows = 0;
static int current_partition_cols = 0;
static int current_partition_count = 0;
static quadrant_mode_t current_partition_modes[25];
static density_level_t current_partition_densities[25];
static big_cave_type_t current_partition_big_cave_types[25];

/* Per-depth big cave type weights (ICE/FIRE/POIS); when unset, default to equal odds. */
static bool g_big_cave_type_rule_set[32];
static int g_big_cave_type_weight[32][BIG_CAVE_TYPE_MAX];

/* Track labyrinth partition count for boosting monsters and stairs in mazes */
static int current_labyrinth_partitions = 0;

/* Morgoth throne room placement state for the current generation attempt */
static bool morgoth_level_active = false;
static bool morgoth_partition_reserved = false;
static int morgoth_partition_index = -1;
static rectangle morgoth_partition_bounds;
static int morgoth_vault_center_y = 0;
static int morgoth_vault_center_x = 0;

static bool morgoth_region_active(void)
{
    return morgoth_level_active && morgoth_partition_reserved;
}

static bool coord_in_morgoth_region(int y, int x, int margin)
{
    if (!morgoth_region_active())
        return false;

    return (y >= morgoth_partition_bounds.y1 - margin)
        && (y <= morgoth_partition_bounds.y2 + margin)
        && (x >= morgoth_partition_bounds.x1 - margin)
        && (x <= morgoth_partition_bounds.x2 + margin);
}

/* Axis-aligned segment vs. Morgoth region intersection */
static bool morgoth_segment_blocked(int y1, int x1, int y2, int x2, int margin)
{
    if (!morgoth_region_active())
        return false;

    /* Quick reject if both points are completely outside in same half-plane */
    if (y1 < morgoth_partition_bounds.y1 - margin && y2 < morgoth_partition_bounds.y1 - margin)
        return false;
    if (y1 > morgoth_partition_bounds.y2 + margin && y2 > morgoth_partition_bounds.y2 + margin)
        return false;
    if (x1 < morgoth_partition_bounds.x1 - margin && x2 < morgoth_partition_bounds.x1 - margin)
        return false;
    if (x1 > morgoth_partition_bounds.x2 + margin && x2 > morgoth_partition_bounds.x2 + margin)
        return false;

    /* Horizontal segment */
    if (y1 == y2)
    {
        int y = y1;
        int xa = MIN(x1, x2);
        int xb = MAX(x1, x2);
        int rx1 = morgoth_partition_bounds.x1 - margin;
        int rx2 = morgoth_partition_bounds.x2 + margin;
        int ry1 = morgoth_partition_bounds.y1 - margin;
        int ry2 = morgoth_partition_bounds.y2 + margin;
        if (y >= ry1 && y <= ry2 && xb >= rx1 && xa <= rx2)
            return true;
    }

    /* Vertical segment */
    if (x1 == x2)
    {
        int x = x1;
        int ya = MIN(y1, y2);
        int yb = MAX(y1, y2);
        int rx1 = morgoth_partition_bounds.x1 - margin;
        int rx2 = morgoth_partition_bounds.x2 + margin;
        int ry1 = morgoth_partition_bounds.y1 - margin;
        int ry2 = morgoth_partition_bounds.y2 + margin;
        if (x >= rx1 && x <= rx2 && yb >= ry1 && ya <= ry2)
            return true;
    }

    return false;
}

void big_cave_type_rules_clear(void)
{
    for (int d = 0; d < 32; ++d) {
        g_big_cave_type_rule_set[d] = false;
        for (int t = 0; t < BIG_CAVE_TYPE_MAX; ++t)
            g_big_cave_type_weight[d][t] = 0;
    }
}

void big_cave_type_set_rule(int depth, int ice_weight, int fire_weight, int pois_weight)
{
    if (depth < 0 || depth >= 32) return;
    g_big_cave_type_rule_set[depth] = true;
    g_big_cave_type_weight[depth][BIG_CAVE_ICE] = MAX(0, ice_weight);
    g_big_cave_type_weight[depth][BIG_CAVE_FIRE] = MAX(0, fire_weight);
    g_big_cave_type_weight[depth][BIG_CAVE_POIS] = MAX(0, pois_weight);
}

big_cave_type_t big_cave_type_pick_for_depth(int depth)
{
    int d = depth;
    if (d < 0) d = 0;
    if (d >= 32) d = 31;

    int ice_w = 1;
    int fire_w = 1;
    int pois_w = 1;
    if (g_big_cave_type_rule_set[d]) {
        ice_w = g_big_cave_type_weight[d][BIG_CAVE_ICE];
        fire_w = g_big_cave_type_weight[d][BIG_CAVE_FIRE];
        pois_w = g_big_cave_type_weight[d][BIG_CAVE_POIS];
    }

    int total = ice_w + fire_w + pois_w;
    if (total <= 0) {
        ice_w = fire_w = pois_w = 1;
        total = 3;
    }

    int r = rand_int(total);
    if (r < ice_w) return BIG_CAVE_ICE;
    r -= ice_w;
    if (r < fire_w) return BIG_CAVE_FIRE;
    return BIG_CAVE_POIS;
}

/* After placing Morgoth's vault, seal a small buffer around it with permanent walls.
 * This prevents accidental extra entrances while keeping the rest of the reserved
 * partition traversable for normal dungeon connectivity. */
static void seal_morgoth_partition(const vault_type* v_ptr, int y0, int x0)
{
    if (!morgoth_region_active() || !v_ptr)
        return;

    int top_y = y0 - v_ptr->hgt / 2;
    int bot_y = top_y + v_ptr->hgt - 1;
    int left_x = x0 - v_ptr->wid / 2;
    int right_x = left_x + v_ptr->wid - 1;

    /* Extend sealing bounds to include the full tunnel path northward.
     * Tunnels are carved to partition_bounds.y1 - 2, so seal from there. */
    int tunnel_limit = morgoth_partition_bounds.y1 - 2;
    if (tunnel_limit < 1) tunnel_limit = 1;
    
    int margin = 4;
    int y1 = MAX(1, MIN(morgoth_partition_bounds.y1, tunnel_limit));  /* Include tunnel area */
    int y2 = MIN(morgoth_partition_bounds.y2, bot_y + margin);
    int x1 = MAX(morgoth_partition_bounds.x1, left_x - margin);
    int x2 = MIN(morgoth_partition_bounds.x2, right_x + margin);

    /* Update the active Morgoth "no-go" bounds to protect the tunnels too. */
    morgoth_partition_bounds.y1 = y1;
    morgoth_partition_bounds.y2 = y2;
    morgoth_partition_bounds.x1 = x1;
    morgoth_partition_bounds.x2 = x2;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            /* Preserve the vault area and any carved entry tunnels */
            if (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                continue;

            /* Don't seal over existing passable floor - only convert walls/granite.
             * This prevents trapping unreachable floor areas that were built before
             * the vault placement. */
            if (cave_floor_bold(y, x) || (cave_feat[y][x] >= FEAT_DOOR_HEAD && cave_feat[y][x] <= FEAT_DOOR_TAIL))
                continue;

            cave_set_feat(y, x, FEAT_WALL_PERM);
            cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);
        }
    }
}

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

static void remember_partition_grid(int rows, int cols, int count)
{
    current_partition_rows = rows;
    current_partition_cols = cols;
    current_partition_count = count;
    for (int i = 0; i < 25; ++i)
    {
        current_partition_modes[i] = QUAD_MODE_ROOMY;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
    }
}

static void record_partition_metadata(
    const quadrant_mode_t* modes, const density_level_t* densities, int count)
{
    int capped = MIN(count, 25);
    for (int i = 0; i < capped; ++i)
    {
        current_partition_modes[i] = modes[i];
        current_partition_densities[i] = densities[i];
    }
}

static void reset_morgoth_layout_state(bool active)
{
    morgoth_level_active = active;
    morgoth_partition_reserved = false;
    morgoth_partition_index = -1;
    morgoth_partition_bounds.y1 = 0;
    morgoth_partition_bounds.y2 = 0;
    morgoth_partition_bounds.x1 = 0;
    morgoth_partition_bounds.x2 = 0;
    morgoth_vault_center_y = 0;
    morgoth_vault_center_x = 0;
}

static void fallback_partition_grid_from_blocks(int blocks, int *rows, int *cols)
{
    /* Mirror the choices in apply_quadrant_generation_modes but without randomness */
    if (blocks <= 9) {
        *rows = 2; *cols = 2;
    } else if (blocks == 10) {
        *rows = 3; *cols = 2;
    } else if (blocks <= 13) {
        *rows = 3; *cols = 3;
    } else if (blocks == 14) {
        *rows = 3; *cols = 4;
    } else if (blocks <= 16) {
        *rows = 4; *cols = 4;
    } else if (blocks <= 20) {
        *rows = 5; *cols = 4;
    } else {
        *rows = 5; *cols = 5;
    }
}

/* Quick scan to see if a region is already reserved/occupied heavily (quest vaults, prefab icky) */
static bool area_is_reserved_or_dense(int y1, int y2, int x1, int x2, int *floor_pct_out, int *icky_pct_out)
{
    int tiles = 0, icky = 0, floors = 0;
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            if (!in_bounds_fully(y, x)) continue;
            tiles++;
            if (cave_info[y][x] & CAVE_ICKY) icky++;
            if (cave_floor_bold(y, x)) floors++;
        }
    }
    int floor_pct = (tiles > 0) ? (floors * 100) / tiles : 0;
    int icky_pct = (tiles > 0) ? (icky * 100) / tiles : 0;
    if (floor_pct_out) *floor_pct_out = floor_pct;
    if (icky_pct_out) *icky_pct_out = icky_pct;

    /* Only treat as reserved if the area is heavily occupied */
    if (floor_pct >= 80) return true;       /* >80% carved already */
    if (icky_pct >= 60) return true;        /* quest/vault dominates area */
    return false;
}

/* Bounded placement helper used by partitions (prototype for early use) */
static bool room_build_in_bounds(int typ, int y1, int y2, int x1, int x2);

/* Partition helper: compute bounds for a given partition index */
static bool compute_partition_bounds(int pi, int rows, int cols, int *y1, int *y2, int *x1, int *x2)
{
    if (rows <= 0 || cols <= 0)
        return false;
    int total = rows * cols;
    if (pi < 0 || pi >= total)
        return false;

    int row = pi / cols;
    int col = pi % cols;

    int ly1 = (row * p_ptr->cur_map_hgt / rows);
    int ly2 = ((row + 1) * p_ptr->cur_map_hgt / rows);
    int lx1 = (col * p_ptr->cur_map_wid / cols);
    int lx2 = ((col + 1) * p_ptr->cur_map_wid / cols);

    if (ly1 < 1) ly1 = 1;
    if (lx1 < 1) lx1 = 1;
    if (ly2 >= p_ptr->cur_map_hgt - 1) ly2 = p_ptr->cur_map_hgt - 2;
    if (lx2 >= p_ptr->cur_map_wid - 1) lx2 = p_ptr->cur_map_wid - 2;

    *y1 = ly1;
    *y2 = ly2;
    *x1 = lx1;
    *x2 = lx2;
    return true;
}

/* Ensure per-tile chasm partition tag is present across the entire partition bounds.
 * This allows gameplay effects (e.g. light absorption) to key off CAVE_CHASM_AREA
 * reliably, including on tiles later converted by digging/tunneling. */
static void apply_chasm_partition_tags(void)
{
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        return;

    for (int pi = 0; pi < current_partition_count; ++pi)
    {
        if (current_partition_modes[pi] != QUAD_MODE_CHASM)
            continue;

        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        if (!compute_partition_bounds(pi, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
            continue;

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!in_bounds(y, x))
                    continue;
                cave_info[y][x] |= CAVE_CHASM_AREA;
            }
        }
    }
}

/* Gameplay lighting rules:
 * - Labyrinth partitions are always dark (no permanent CAVE_GLOW).
 * - CA_BLOB rooms ("caves") are always dark (no permanent CAVE_GLOW). */
static void apply_partition_and_room_glow_rules(void)
{
    /* Labyrinth partitions: clear CAVE_GLOW across full partition bounds. */
    if (current_partition_rows > 0 && current_partition_cols > 0 && current_partition_count > 0)
    {
        for (int pi = 0; pi < current_partition_count; ++pi)
        {
            if (current_partition_modes[pi] != QUAD_MODE_LABYRINTH)
                continue;

            int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
            if (!compute_partition_bounds(pi, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
                continue;

            for (int y = y1; y <= y2; ++y)
                for (int x = x1; x <= x2; ++x)
                    cave_info[y][x] &= ~(CAVE_GLOW);
        }
    }

    /* CA_BLOB rooms: clear CAVE_GLOW in room bounds (expanded by 1 to cover the
     * typical outer wall ring used by rectangular room builders). */
    for (int r = 0; r < dun->cent_n; ++r)
    {
        if (room_anchor_kind[r] != LAYOUT_ANCHOR_CA_BLOB)
            continue;

        int y1 = dun->corner[r].y1 - 1;
        int y2 = dun->corner[r].y2 + 1;
        int x1 = dun->corner[r].x1 - 1;
        int x2 = dun->corner[r].x2 + 1;

        if (y1 < 0) y1 = 0;
        if (x1 < 0) x1 = 0;
        if (y2 >= MAX_DUNGEON_HGT) y2 = MAX_DUNGEON_HGT - 1;
        if (x2 >= MAX_DUNGEON_WID) x2 = MAX_DUNGEON_WID - 1;

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!in_bounds(y, x))
                    continue;
                cave_info[y][x] &= ~(CAVE_GLOW);
            }
        }
    }
}

/* Extra monsters in non-roomy partitions (prevents huge caves/chasm/ruins from feeling empty). */
static int partition_extra_monster_target(quadrant_mode_t mode, int floor_count)
{
    int target = 0;

    switch (mode)
    {
    case QUAD_MODE_BIG_CAVE:
        /* Big caves are huge: aim for a noticeable population.
         * Divisor lowered from 60 -> 45 to offset loss of group spawning. */
        target = floor_count / 45;
        if (target < 12) target = 12;
        if (target > 40) target = 40;
        break;
    case QUAD_MODE_CHASM:
        /* grp=false: divisor tightened from 50->38 to compensate. */
        target = floor_count / 38;
        if (target < 10) target = 10;
        if (target > 40) target = 40;
        break;
    case QUAD_MODE_LABYRINTH:
        /* Labyrinths already place monsters during carving; mild boost for grp=false. */
        target = floor_count / 80;
        if (target > 25) target = 25;
        break;
    case QUAD_MODE_CAVEY:
        /* grp=false: divisor tightened from 170->120. */
        target = floor_count / 120;
        if (target > 18) target = 18;
        break;
    case QUAD_MODE_RUINED:
        /* grp=false: divisor tightened from 260->180. */
        target = floor_count / 180;
        if (target > 14) target = 14;
        break;
    default:
        target = 0;
        break;
    }

    if (target <= 0)
        return 0;

    /* Gentle depth scaling (caps at +50%). */
    int scale = 100 + MIN(50, p_ptr->depth * 2);
    target = target * scale / 100;

    return target;
}

static bool place_monster_by_flag_try(int y, int x, int flagset, u32b flag, bool allow_unique, int max_depth);
static bool place_monster_by_letter_try(int y, int x, char letter, bool allow_unique, int max_depth);
static bool place_big_cave_elemental_monster(int y, int x, big_cave_type_t cave_type, int max_depth);
static bool place_big_cave_troll_or_giant(int y, int x, int max_depth);

static int place_partition_extra_monsters(void)
{
    if (morgoth_level_active)
        return 0;

    if (current_partition_count <= 0 || current_partition_rows <= 0 || current_partition_cols <= 0)
        return 0;

    int total_placed = 0;

    for (int pi = 0; pi < current_partition_count; ++pi)
    {
        quadrant_mode_t mode = current_partition_modes[pi];
        if (mode == QUAD_MODE_ROOMY)
            continue;

        int y1, y2, x1, x2;
        if (!compute_partition_bounds(pi, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
            continue;

        int floor_count = 0;
        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                if (!in_bounds_fully(y, x))
                    continue;

                if (!cave_floor_bold(y, x))
                    continue;

                /* Don't bias further into vaults/quest areas. */
                if (cave_info[y][x] & CAVE_ICKY)
                    continue;

                floor_count++;
            }
        }

        int target = partition_extra_monster_target(mode, floor_count);
        if (target <= 0)
            continue;

        /* Safety: never exceed 1 monster per ~20 floor tiles. */
        int hard_cap = floor_count / 20;
        if (hard_cap < 1) hard_cap = 1;
        if (target > hard_cap)
            target = hard_cap;

        int placed = 0;
        int attempts = target * 250;

        for (int tries = 0; tries < attempts && placed < target; ++tries)
        {
            int y = rand_range(y1, y2);
            int x = rand_range(x1, x2);

            if (!in_bounds_fully(y, x))
                continue;

            if (cave_info[y][x] & CAVE_ICKY)
                continue;

            if (!cave_naked_bold(y, x))
                continue;

            /* Match alloc_monster() feel: don't spawn in the player's initial LOS. */
            if (los(p_ptr->py, p_ptr->px, y, x))
                continue;

            /* Single monsters (no explicit group placement) to avoid runaway density. */
            bool placed_mon = false;
            if (mode == QUAD_MODE_BIG_CAVE)
            {
                int pref_roll = rand_int(100);
                if (pref_roll < 45)
                    placed_mon = place_big_cave_elemental_monster(
                        y, x, current_partition_big_cave_types[pi], p_ptr->depth);
                if (!placed_mon && pref_roll < 70)
                    placed_mon = place_big_cave_troll_or_giant(y, x, p_ptr->depth);
            }
            else if (mode == QUAD_MODE_CHASM)
            {
                if (rand_int(100) < 55)
                    placed_mon = place_monster_by_letter_try(y, x, 'w', true, p_ptr->depth);
            }
            else if (mode == QUAD_MODE_LABYRINTH)
            {
                if (rand_int(100) < 55)
                    placed_mon = place_monster_by_flag_try(y, x, 2, RF2_INVISIBLE, true, p_ptr->depth);
            }
            else if (mode == QUAD_MODE_CAVEY)
            {
                int pref_roll = rand_int(100);
                if (pref_roll < 45)
                    placed_mon = place_monster_by_letter_try(y, x, 'M', false, p_ptr->depth);
                else if (pref_roll < 70)
                    placed_mon = place_monster_by_letter_try(y, x, 'C', false, p_ptr->depth);
                else if (pref_roll < 90)
                    placed_mon = place_monster_by_letter_try(y, x, 'b', false, p_ptr->depth);
                else
                    placed_mon = place_monster_by_letter_try(y, x, 'T', false, p_ptr->depth);
            }

            if (!placed_mon)
                placed_mon = place_monster(y, x, true, false, false);

            if (placed_mon)
            {
                placed++;
                total_placed++;
            }
        }

        log_trace("Extra partition monsters: pi=%d mode=%d bounds=(%d,%d)-(%d,%d) floors=%d target=%d placed=%d",
                  pi, mode, y1, x1, y2, x2, floor_count, target, placed);
    }

    return total_placed;
}

/* Gentle scaling helper: add ~50% per size tier (caps explosive growth) */
static int scaled_attempts(int base, int area_factor)
{
    if (area_factor <= 1) return base;
    int extra = (base + 1) / 2;
    return base + extra * (area_factor - 1);
}

/* Ensure modes with few exits expose boundary openings.
 * FIXED: Only create openings on sides that border other partitions, not map edges.
 * This prevents dead-end corridors at the map boundary. */
static void ensure_partition_boundary_openings(int y1, int y2, int x1, int x2, int openings)
{
    /* Determine which sides border other partitions (not map edge) */
    bool can_open_top = (y1 > 5);     /* Top side borders another partition */
    bool can_open_bot = (y2 < p_ptr->cur_map_hgt - 5);  /* Bottom side borders another partition */
    bool can_open_left = (x1 > 5);    /* Left side borders another partition */
    bool can_open_right = (x2 < p_ptr->cur_map_wid - 5); /* Right side borders another partition */
    
    int valid_sides[4];
    int valid_count = 0;
    if (can_open_top) valid_sides[valid_count++] = 0;
    if (can_open_bot) valid_sides[valid_count++] = 1;
    if (can_open_left) valid_sides[valid_count++] = 2;
    if (can_open_right) valid_sides[valid_count++] = 3;
    
    if (valid_count == 0)
        return;  /* No valid sides to open (corner partition at map edge) */
    
    for (int i = 0; i < openings; ++i)
    {
        int side = valid_sides[rand_int(valid_count)];
        int y = rand_range(y1 + 2, y2 - 2);
        int x = rand_range(x1 + 2, x2 - 2);

        switch (side)
        {
        case 0: y = y1; break;         /* top */
        case 1: y = y2; break;         /* bottom */
        case 2: x = x1; break;         /* left */
        case 3: x = x2; break;         /* right */
        }

        if (in_bounds_fully(y, x) && cave_feat[y][x] != FEAT_WALL_PERM)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
        }
    }
}

static quadrant_mode_t pick_weighted_mode(const int *weights, int count)
{
    int total = 0;
    for (int i = 0; i < count; ++i)
        total += MAX(0, weights[i]);
    if (total <= 0)
        return QUAD_MODE_ROOMY;
    int roll = rand_int(total);
    for (int i = 0; i < count; ++i) {
        int w = MAX(0, weights[i]);
        if (roll < w)
            return (quadrant_mode_t)i;
        roll -= w;
    }
    return QUAD_MODE_ROOMY;
}

static int special_mode_start_depth(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_LABYRINTH:
        return LABYRINTH_START_DEPTH;
    case QUAD_MODE_BIG_CAVE:
        return BIG_CAVE_START_DEPTH;
    case QUAD_MODE_CHASM:
        return CHASM_START_DEPTH;
    default:
        return 0;
    }
}

static int mode_cap_for_depth(
    quadrant_mode_t mode, int depth, int partition_count)
{
    int start = special_mode_start_depth(mode);
    if (start <= 0)
        return partition_count;
    if (depth < start)
        return 0;

    int cap = 1 + (depth - start) / SPECIAL_CAP_STEP;
    if (cap > SPECIAL_CAP_MAX)
        cap = SPECIAL_CAP_MAX;
    return cap;
}

static int mode_weight_for_depth(quadrant_mode_t mode, int depth, int blocks,
    const int* mode_counts, int partition_count)
{
    int cap = mode_cap_for_depth(mode, depth, partition_count);
    if (cap == 0)
        return 0;
    if (mode_counts && mode_counts[mode] >= cap)
        return 0;

    (void)blocks; /* No longer used for scaling */

    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return 25;
    case QUAD_MODE_CAVEY:
        /* Increase up to depth 12, then decrease */
        if (depth <= 12)
            return 15 + depth;  /* 15 at depth 0, 27 at depth 12 */
        else
            return MAX(5, 27 - (depth - 12));  /* Decrease after 12, minimum 5 */
    case QUAD_MODE_RUINED:
        /* Decrease with depth */
        return MAX(5, 15 + 10 - depth);  /* 25 at depth 0, decreases to minimum 5 */
    case QUAD_MODE_LABYRINTH:
        /* Increase with depth (starts at depth 7) */
        return 10 + MAX(0, depth - LABYRINTH_START_DEPTH);
    case QUAD_MODE_BIG_CAVE:
        /* Increase with depth (starts at depth 10) */
        return 8 + MAX(0, depth - BIG_CAVE_START_DEPTH);
    case QUAD_MODE_CHASM:
        /* Increase with depth (starts at depth 14) */
        return 8 + MAX(0, depth - CHASM_START_DEPTH);
    default:
        return 0;
    }
}

/* Budget-aware placement helper with limited retries */
static bool place_room_with_budget(int typ, int y1, int y2, int x1, int x2, int max_tries, int depth,
    int *budget_t6, int *budget_t7, int *budget_t8, int *used_t6, int *used_t7, int *used_t8)
{
    int actual = typ;
    (void)depth; /* unused after removing GV promotion */

    if (actual == 8 && (!budget_t8 || *budget_t8 <= 0))
        actual = (budget_t7 && *budget_t7 > 0) ? 7 : ((budget_t6 && *budget_t6 > 0) ? 6 : 2);
    if (actual == 7 && budget_t7 && *budget_t7 <= 0)
        actual = (budget_t6 && *budget_t6 > 0) ? 6 : 2;
    if (actual == 6 && budget_t6 && *budget_t6 <= 0)
        actual = 2;  /* downgrade to simple cross room if out of budget */

    for (int attempt = 0; attempt < max_tries; ++attempt) {
        if (room_build_in_bounds(actual, y1, y2, x1, x2)) {
            if (actual == 6 && budget_t6 && *budget_t6 > 0) { (*budget_t6)--; if (used_t6) (*used_t6)++; }
            else if (actual == 7 && budget_t7 && *budget_t7 > 0) { (*budget_t7)--; if (used_t7) (*used_t7)++; }
            else if (actual == 8 && budget_t8 && *budget_t8 > 0) { (*budget_t8)--; if (used_t8) (*used_t8)++; }
            return true;
        }
    }
    return false;
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
static bool build_type9(int y0, int x0, vault_type** used_vault);
static bool build_type2(int y0, int x0);
static bool build_type1(int y0, int x0);
static void carve_morgoth_entry_tunnels(const vault_type* v_ptr, int y0, int x0);
static void connect_morgoth_entry_tunnels(void);
static void seal_morgoth_partition(const vault_type* v_ptr, int y0, int x0);
static int place_ruined_partition_damaged_items(
    int y1, int y2, int x1, int x2, int target_count);
static void apply_quadrant_generation_modes(void);
static void repair_all_outer_walls(void);
static bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max,
    int floor_style, int bridge_style);
static void cave_set_feat_style(int y, int x, int feat, int style_idx);
static int dungeon_pieces(void);
static int partition_index_from_point(int y, int x, int rows, int cols);
static int room_connection_degree(int room_idx);
static bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate);
static bool connect_two_rooms(int r1, int r2, bool tentative, bool desperate);
static bool compute_partition_bounds(int pi, int rows, int cols, int *y1, int *y2, int *x1, int *x2);
static void connect_partition_hubs(void);
static bool feature_is_any_door(int feat);

/* Disabled helpers kept for reference (see #if 0 blocks near usage sites). */
#if 0
static void seed_ca_blob_anchors(void);
static void seed_bsp_slice_anchors(void);
static void ensure_partition_connectivity(void);
#endif

/* Attempt to place a prefab vault/room as a generation anchor */
static bool place_prefab_anchor_of_type(int typ, bool require_neighbor)
{
    if (dun->cent_n >= room_capacity_limit())
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

/* Scatter quartz veins around CA blob edges to give a natural cave-like appearance.
 * Converts some wall tiles adjacent to floors into quartz veins. */
static void scatter_quartz_veins_in_bounds(int y1, int y2, int x1, int x2, u16b info_flag)
{
    int vein_count = 0;
    
    /* Iterate over the bounds and convert some adjacent-to-floor walls to quartz */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            
            /* Only consider granite walls */
            int feat = cave_feat[gy][gx];
            if (feat < FEAT_WALL_EXTRA || feat > FEAT_WALL_SOLID)
                continue;
            
            /* Check if adjacent to at least one cave floor tile (CAVE_ROOM) */
            bool adj_cave_floor = false;
            for (int dy = -1; dy <= 1 && !adj_cave_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !adj_cave_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        adj_cave_floor = true;
                }
            }
            
            /* If adjacent to cave floor, ~30% chance to become quartz vein */
            if (adj_cave_floor && (rand_int(100) < 30))
            {
                cave_set_feat(gy, gx, FEAT_QUARTZ);
                /* Mark as part of a room so tunneling can detect cave quartz */
                cave_info[gy][gx] |= (CAVE_ROOM | info_flag);
                vein_count++;
            }
        }
    }
    
    if (vein_count > 0)
    {
        log_trace("scatter_quartz_veins: placed %d veins in bounds (%d,%d)-(%d,%d)",
                  vein_count, y1, x1, y2, x2);
    }
}

/* Scatter mithril pieces and gem caches in cave areas.
 * Chance increases with depth and blob count. Only places items on empty floor tiles. */
static void scatter_cave_gems_in_bounds(int y1, int y2, int x1, int x2, bool is_big_cave, int blob_count)
{
    int depth = p_ptr->depth;
    int mithril_placed = 0;
    int gem_placed = 0;
    bool allow_mithril = (depth >= 8);
    int blob_factor = MAX(1, blob_count);
    int blob_bonus = blob_factor - 1;
    
    if (allow_mithril)
    {
        /* Up to 3 mithril chunks in small caves, 2 in big caves */
        int max_mithril = is_big_cave ? 2 : MIN(3, 1 + (blob_factor + 1) / 2);
        
        /* Higher chance: 15% base + depth/5, scaled by blob_factor, capped */
        int spawn_chance = 15 + (depth / 5) + 5 * (blob_factor - 1);
        int spawn_cap = is_big_cave ? 45 : 60;
        if (spawn_chance > spawn_cap) spawn_chance = spawn_cap;

        /* Big caves have worse odds; regular caves keep the baseline */
        if (is_big_cave)
            spawn_chance = MAX(1, spawn_chance - 2);

        bool try_mithril = (rand_int(100) < spawn_chance);
        
        /* Try to place the mithril on a random floor tile */
        for (int attempt = 0; try_mithril && attempt < 50 && mithril_placed < max_mithril; ++attempt)
        {
            int gy = rand_range(y1, y2);
            int gx = rand_range(x1, x2);
            if (!in_bounds_fully(gy, gx)) continue;
            if (!cave_floor_bold(gy, gx)) continue;
            if (cave_o_idx[gy][gx] != 0) continue;  /* Already has object */
            
            /* Create mithril piece */
            object_type object_type_body;
            object_type *i_ptr = &object_type_body;
            object_wipe(i_ptr);
            
            s16b k_idx = lookup_kind(TV_METAL, SV_METAL_MITHRIL);
            if (k_idx > 0)
            {
                object_prep(i_ptr, k_idx);
                drop_near(i_ptr, -1, gy, gx);
                mithril_placed++;
            }
        }
    }
    
    /* Try to place up to 2 gem caches (rarer in big caves) - scale with blob count */
    int gem_base_chance = is_big_cave ? 45 : 60;
    int gem_chance = gem_base_chance + 6 * blob_bonus;
    if (gem_chance > 90) gem_chance = 90;
    int gem_targets = is_big_cave ? 2 : MIN(3, 1 + (blob_factor + 1) / 2);
    int gems_tried = 0;
    while (gems_tried < gem_targets && rand_int(100) < gem_chance)
    {
        gems_tried++;
        for (int attempt = 0; attempt < 60 && gem_placed < gem_targets; ++attempt)
        {
            int gy = rand_range(y1, y2);
            int gx = rand_range(x1, x2);
            if (!in_bounds_fully(gy, gx)) continue;
            if (!cave_floor_bold(gy, gx)) continue;
            if (cave_o_idx[gy][gx] != 0) continue;

            object_type object_type_body;
            object_type *i_ptr = &object_type_body;
            object_wipe(i_ptr);

            drop_profile gem_profile;
            drop_profile_default(&gem_profile);
            gem_profile.weight_weapon = 0;
            gem_profile.weight_armor = 0;
            gem_profile.weight_jewelry = 0;
            gem_profile.weight_supply = 120;
            gem_profile.supply_potion = 3;
            gem_profile.supply_herb = 3;
            gem_profile.supply_gem = 40;
            gem_profile.supply_staff = 4;
            gem_profile.supply_misc = 2;

            if (drop_generate_object_profiled(depth, DROP_QUALITY_NORMAL,
                    DROP_TYPE_STAFF, 0, false, &gem_profile, i_ptr))
            {
                drop_near(i_ptr, -1, gy, gx);
                gem_placed++;
                break;
            }
        }
    }

    /* Scatter torches; both wooden and mallorn in small caves only, max 2 together */
    if (!is_big_cave)
    {
        int torch_chance = 50;
        int torch_max = 2;
        int torch_placed = 0;
        if (rand_int(100) < torch_chance)
        {
            for (int attempt = 0; attempt < 80 && torch_placed < torch_max; ++attempt)
            {
                int gy = rand_range(y1, y2);
                int gx = rand_range(x1, x2);
                if (!in_bounds_fully(gy, gx)) continue;
                if (!cave_floor_bold(gy, gx)) continue;
                if (cave_o_idx[gy][gx] != 0) continue;

                object_type object_type_body;
                object_type *i_ptr = &object_type_body;
                object_wipe(i_ptr);

                /* 10% chance to drop a digging tool instead of a torch */
                int droptype = (rand_int(100) < 10) ? DROP_TYPE_DIGGING : DROP_TYPE_TORCHES;
                if (drop_generate_object(depth, DROP_QUALITY_NORMAL, droptype, false, i_ptr)
                    || (droptype == DROP_TYPE_DIGGING
                        && drop_generate_object(depth, DROP_QUALITY_NORMAL, DROP_TYPE_TORCHES,
                            false, i_ptr)))
                {
                    drop_near(i_ptr, -1, gy, gx);
                    torch_placed++;
                }
            }
        }
    }

    if (mithril_placed > 0 || gem_placed > 0)
    {
        log_trace("scatter_cave_mithril: mithril=%d gems=%d bounds (%d,%d)-(%d,%d) depth=%d",
                  mithril_placed, gem_placed, y1, x1, y2, x2, depth);
    }
}

static bool bounds_have_chasm_tag(int y1, int y2, int x1, int x2)
{
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx) && (cave_info[gy][gx] & CAVE_CHASM_AREA))
                return true;
        }
    }
    return false;
}

/* Scatter star-iron pieces across chasm partitions (on floor tiles only). */
static void scatter_chasm_star_iron_in_bounds(int y1, int y2, int x1, int x2)
{
    int depth = p_ptr->depth;
    if (depth < 8)
        return;

    int area = (y2 - y1 + 1) * (x2 - x1 + 1);
    int size_factor = MAX(1, area / 500);
    if (size_factor > 4) size_factor = 4;
    bool require_chasm_tag = bounds_have_chasm_tag(y1, y2, x1, x2);

    /* Up to 4 chunks, scaled by partition size */
    int max_chunks = MIN(4, 1 + size_factor);

    /* Depth-scaled chance, mirroring mithril cave logic */
    int spawn_chance = 16 + (depth / 5) + 6 * (size_factor - 1);
    int spawn_cap = 55;
    if (spawn_chance > spawn_cap)
        spawn_chance = spawn_cap;

    int placed = 0;
    bool try_star = (rand_int(100) < spawn_chance);
    for (int attempt = 0; try_star && attempt < 60 && placed < max_chunks; ++attempt)
    {
        int gy = rand_range(y1, y2);
        int gx = rand_range(x1, x2);
        if (!in_bounds_fully(gy, gx))
            continue;
        if (!cave_floor_bold(gy, gx))
            continue;
        /* Only drop inside the chasm partition when tagged */
        if (require_chasm_tag && !(cave_info[gy][gx] & CAVE_CHASM_AREA))
            continue;
        if (cave_o_idx[gy][gx] != 0)
            continue;

        s16b k_idx = lookup_kind(TV_METAL, SV_METAL_STAR_IRON);
        if (k_idx <= 0)
            break;

        object_type object_type_body;
        object_type *i_ptr = &object_type_body;
        object_wipe(i_ptr);
        object_prep(i_ptr, k_idx);
        drop_near(i_ptr, -1, gy, gx);
        placed++;
    }

    if (placed > 0)
    {
        log_trace("scatter_chasm_star_iron: pieces=%d bounds (%d,%d)-(%d,%d) depth=%d",
                  placed, y1, x1, y2, x2, depth);
    }
}

/* Carve a small cellular-automata style blob and register it as an anchor */
#if 0
static bool carve_ca_blob_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    /* Pick blob dimensions (moderate footprint to avoid over-densifying) */
    int h = rand_range(8, 12);
    int w = rand_range(10, 16);
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

    /* Seed noise with a bias to produce irregular shapes */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = (rand_int(100) < 45); /* 45% initial fill */

    /* Run several smoothing steps to create rounded blobs */
    int steps = 3;
    for (int step = 0; step < steps; ++step)
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
                /* Slightly denser survival/birth to keep blobs cohesive */
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
    /* Clear box to raw granite to avoid rectangular outlines */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            if (in_bounds_fully(gy, gx))
                cave_set_feat(gy, gx, FEAT_WALL_EXTRA);

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

    /* Ragged edge expansion to break rectangular silhouette */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        {
            for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            {
                if (cave_floor_bold(gy, gx))
                    continue;
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dy || dx)
                        {
                            int ny = gy + dy, nx = gx + dx;
                            if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                                adj++;
                        }
                if (adj >= 3 && one_in_(2 + pass))
                {
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
        }
    }

    /* Bleed outward a little to break boxy outlines */
    const int bleed_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!cave_floor_bold(gy, gx))
                continue;
            bool on_edge = (gy == y1 - 1) || (gy == y2 + 1) || (gx == x1 - 1) || (gx == x2 + 1);
            if (!on_edge)
                continue;
            for (int d = 0; d < 4; ++d)
            {
                int ny = gy + bleed_dirs[d][0];
                int nx = gx + bleed_dirs[d][1];
                if (!in_bounds_fully(ny, nx))
                    continue;
                if (cave_floor_bold(ny, nx))
                    continue;
                if (cave_feat[ny][nx] != FEAT_WALL_EXTRA)
                    continue;
                if (one_in_(4))
                {
                    cave_set_feat(ny, nx, FEAT_FLOOR);
                    cave_info[ny][nx] |= CAVE_ROOM;
                    floor_count++;
                    if (ny < min_y)
                        min_y = ny;
                    if (ny > max_y)
                        max_y = ny;
                    if (nx < min_x)
                        min_x = nx;
                    if (nx > max_x)
                        max_x = nx;
                }
            }
        }
    }

    if (floor_count < 8)
        return false;

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
        }
    }

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

    /* Scatter quartz veins around the cave walls for natural appearance */
    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("CA blob anchor: carved floor_count=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    genlog_anchor("CA_BLOB: carved %d floor tiles at (%d,%d)-(%d,%d), center=(%d,%d)",
        floor_count, min_y, min_x, max_y, max_x, cy, cx);
    return true;
}
#endif

/* Bounded version for quadrants */
static bool carve_ca_blob_anchor_bounds(int y_min, int y_max, int x_min, int x_max, int style_idx)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    int old_h = p_ptr->cur_map_hgt;
    int old_w = p_ptr->cur_map_wid;
    /* Temporarily clamp selection by picking starting coordinates inside bounds */
    if (y_max - y_min < 8 || x_max - x_min < 8)
        return false;
    int h = rand_range(8, MIN(14, y_max - y_min));
    int w = rand_range(10, MIN(16, x_max - x_min));
    int y1 = rand_range(y_min + 1, y_max - h);
    int x1 = rand_range(x_min + 1, x_max - w);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (y1 < 1 || x1 < 1 || y2 >= old_h - 1 || x2 >= old_w - 1)
        return false;
    for (int y = y1 - 1; y <= y2 + 1; ++y)
        for (int x = x1 - 1; x <= x2 + 1; ++x)
            if (cave_floor_bold(y, x))
                return false;

    /* Simple CA grid stored on stack */
    bool grid[24][24];
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            grid[y][x] = (rand_int(100) < 45);

    int steps = 3;
    for (int step = 0; step < steps; ++step)
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
                        if (dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (grid[ny][nx])
                            neighbors++;
                    }
                }
                next[y][x] = grid[y][x] ? (neighbors >= 4) : (neighbors >= 5);
            }
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                grid[y][x] = next[y][x];
    }

    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    int floor_count = 0;
    /* Clear box to raw granite to avoid rectangular outlines */
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            if (in_bounds_fully(gy, gx))
                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!grid[y][x]) continue;
            int gy = y1 + y;
            int gx = x1 + x;
            cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
            cave_info[gy][gx] |= CAVE_ROOM;
            floor_count++;
            if (gy < min_y) min_y = gy;
            if (gy > max_y) max_y = gy;
            if (gx < min_x) min_x = gx;
            if (gx > max_x) max_x = gx;
        }
    }
    if (floor_count < 8)
        return false;

    /* Ragged edge expansion to break rectangular silhouette */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
        {
            for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
            {
                if (cave_floor_bold(gy, gx))
                    continue;
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dy || dx)
                        {
                            int ny = gy + dy, nx = gx + dx;
                            if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                                adj++;
                        }
                if (adj >= 3 && one_in_(2 + pass))
                {
                    cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
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
        }
    }

    /* Bleed outward along the edge to soften rectangles */
    const int bleed_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int gy = y1 - 1; gy <= y2 + 1; ++gy)
    {
        for (int gx = x1 - 1; gx <= x2 + 1; ++gx)
        {
            if (!cave_floor_bold(gy, gx))
                continue;
            bool on_edge = (gy == y1 - 1) || (gy == y2 + 1) || (gx == x1 - 1) || (gx == x2 + 1);
            if (!on_edge)
                continue;
            for (int d = 0; d < 4; ++d)
            {
                int ny = gy + bleed_dirs[d][0];
                int nx = gx + bleed_dirs[d][1];
                if (!in_bounds_fully(ny, nx))
                    continue;
                if (cave_floor_bold(ny, nx))
                    continue;
                if (cave_feat[ny][nx] != FEAT_WALL_EXTRA)
                    continue;
                if (one_in_(4))
                {
                    cave_set_feat_style(ny, nx, FEAT_FLOOR, style_idx);
                    cave_info[ny][nx] |= CAVE_ROOM;
                    floor_count++;
                    if (ny < min_y)
                        min_y = ny;
                    if (ny > max_y)
                        max_y = ny;
                    if (nx < min_x)
                        min_x = nx;
                    if (nx > max_x)
                        max_x = nx;
                }
            }
        }
    }

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
            }
        }
    }

    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int cy = min_y, cx = min_x;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty; cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty; cx = tx; break;
            }
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
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, one_in_(4));

    /* Scatter quartz veins around the cave walls for natural appearance */
    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);

    log_trace("CA blob (bounded) anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) floors=%d", min_y, min_x, max_y, max_x, cy, cx, floor_count);
    return true;
}

/* Carve a large cave filling most of the given bounds, using cellular automata */
static bool carve_big_cave_bounds(int y_min, int y_max, int x_min, int x_max,
    int style_idx, big_cave_type_t cave_type)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("BIG_CAVE: rejected - room capacity limit reached");
        return false;
    }
    
    /* Big caves need substantial space */
    int avail_h = y_max - y_min;
    int avail_w = x_max - x_min;
    if (avail_h < 15 || avail_w < 20)
    {
        genlog_anchor("BIG_CAVE: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }
    
    /* Use smaller margins to create larger, more expansive caves */
    int margin_y1 = rand_range(2, MAX(4, avail_h / 5));
    int margin_y2 = rand_range(2, MAX(4, avail_h / 5));
    int margin_x1 = rand_range(2, MAX(4, avail_w / 5));
    int margin_x2 = rand_range(2, MAX(4, avail_w / 5));
    int y1 = y_min + margin_y1;
    int x1 = x_min + margin_x1;
    int y2 = y_max - margin_y2;
    int x2 = x_max - margin_x2;
    int h = y2 - y1 + 1;
    int w = x2 - x1 + 1;
    
    if (h < 10 || w < 12)
    {
        genlog_anchor("BIG_CAVE: rejected - after margins too small: h=%d w=%d",
                      h, w);
        return false;
    }
    
    /* Check area is basic granite */
    for (int y = y1 - 1; y <= y2 + 1; ++y)
    {
        for (int x = x1 - 1; x <= x2 + 1; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("BIG_CAVE: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }
    
    /* Let caves grow to fill the partition without artificial size caps */
    /* Removed h>50, w>60 limits for more expansive caves on larger levels */
    
    /* Use multiple overlapping CA blobs to create one large organic cave */
    /* This approach creates natural irregular shapes instead of rectangles */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    
    /* Number of blob centers based on area - more blobs for bigger caves */
    int num_centers = 4 + (h * w) / 150;  /* Increased from 3 + area/200 */
    if (num_centers > 12) num_centers = 12;  /* Raised cap from 8 to 12 */
    
    /* Generate random center points for blob nuclei */
    int centers_y[12], centers_x[12];  /* Increased from 8 to 12 */
    for (int c = 0; c < num_centers; ++c)
    {
        centers_y[c] = rand_range(y1 + 2, y2 - 2);
        centers_x[c] = rand_range(x1 + 2, x2 - 2);
    }
    
    /* Carve floor by distance from nearest center with noise */
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            
            /* Find distance to nearest center */
            int min_dist = 9999;
            for (int c = 0; c < num_centers; ++c)
            {
                int dy = ABS(gy - centers_y[c]);
                int dx = ABS(gx - centers_x[c]);
                int dist = dy + dx;  /* Manhattan distance */
                if (dist < min_dist) min_dist = dist;
            }
            
            /* Carve floor based on distance with randomness for organic edges */
            int threshold = (h + w) / 4;  /* Base carve radius */
            int noise = rand_int(threshold / 2);  /* Add randomness */
            
            if (min_dist < threshold - noise)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    /* Smooth the edges with a CA pass */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int gy = min_y; gy <= max_y; ++gy)
        {
            for (int gx = min_x; gx <= max_x; ++gx)
            {
                if (!in_bounds_fully(gy, gx)) continue;
                if (cave_floor_bold(gy, gx)) continue;
                
                /* Count floor neighbors */
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dy || dx) && in_bounds_fully(gy+dy, gx+dx) && cave_floor_bold(gy+dy, gx+dx))
                            adj++;
                
                /* Fill in isolated wall cells surrounded by floor */
                if (adj >= 6)
                {
                    cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                    cave_info[gy][gx] |= CAVE_ROOM;
                    floor_count++;
                }
            }
        }
    }
    
    /* Erode some edge floor tiles for more irregular shape */
    for (int gy = min_y; gy <= max_y; ++gy)
    {
        for (int gx = min_x; gx <= max_x; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (!cave_floor_bold(gy, gx)) continue;
            
            /* Count wall neighbors */
            int walls = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if ((dy || dx) && in_bounds_fully(gy+dy, gx+dx) && !cave_floor_bold(gy+dy, gx+dx))
                        walls++;
            
            /* Erode edge tiles more aggressively for irregular cave-like edges */
            if (walls >= 3 && rand_int(100) < 45)  /* Increased from one_in_(3) = 33% to 45% */
            {
                cave_set_feat_style(gy, gx, FEAT_WALL_EXTRA, style_idx);
                cave_info[gy][gx] &= ~CAVE_ROOM;
                floor_count--;
            }
        }
    }
    
    /* Recalculate bounds after erosion */
    min_y = y2; max_y = y1; min_x = x2; max_x = x1;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (cave_floor_bold(gy, gx) && (cave_info[gy][gx] & CAVE_ROOM))
            {
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    if (floor_count < 40)
        return false;
    
    /* Add some internal pillars for visual interest */
    int pillar_count = floor_count / 60;
    for (int p = 0; p < pillar_count; ++p)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            int py = rand_range(min_y + 2, max_y - 2);
            int px = rand_range(min_x + 2, max_x - 2);
            if (cave_floor_bold(py, px))
            {
                bool all_floor = true;
                for (int dy = -1; dy <= 1 && all_floor; ++dy)
                    for (int dx = -1; dx <= 1 && all_floor; ++dx)
                        if (!cave_floor_bold(py + dy, px + dx))
                            all_floor = false;
                if (all_floor)
                {
                    cave_set_feat_style(py, px, FEAT_WALL_EXTRA, style_idx);
                    break;
                }
            }
        }
    }
    
    /* Set outer walls */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }
    
    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int cy = (min_y + max_y) / 2, cx = (min_x + max_x) / 2;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty; cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty; cx = tx; break;
            }
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
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, false);
    
    scatter_quartz_veins_in_bounds(min_y, max_y, min_x, max_x, 0);
    /* Treat the big cave as a few merged blobs for loot scaling */
    scatter_cave_gems_in_bounds(min_y, max_y, min_x, max_x, true, 3);  /* Big cave gets extra gems */
    
    /* === BIG CAVE MONSTER SPAWNING === */
    /* Place monsters directly inside the big cave - scale with floor count */
    /* Approximately 1 monster per 50 floor tiles (drastically increased) */
    int cave_monsters = floor_count / 50;
    if (cave_monsters < 10) cave_monsters = 10;
    if (cave_monsters > 40) cave_monsters = 40;
    int monsters_placed = 0;
    
    for (int m = 0; m < cave_monsters; ++m)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int my = rand_range(min_y, max_y);
            int mx = rand_range(min_x, max_x);
            if (!in_bounds_fully(my, mx)) continue;
            if (!cave_floor_bold(my, mx)) continue;
            if (cave_m_idx[my][mx] != 0) continue;  /* Already has monster */
            if (cave_o_idx[my][mx] != 0) continue;  /* Has object */
            
            /* Prefer elemental and troll/giant monsters for big caves. */
            bool placed = false;
            int pref_roll = rand_int(100);
            if (pref_roll < 45)
                placed = place_big_cave_elemental_monster(my, mx, cave_type, p_ptr->depth);
            if (!placed && pref_roll < 70)
                placed = place_big_cave_troll_or_giant(my, mx, p_ptr->depth);
            if (!placed)
                placed = place_monster(my, mx, true, false, false);
            if (placed)
            {
                monsters_placed++;
                break;
            }
        }
    }
    
    /* === BIG CAVE SKELETON SPAWNING === */
    /* Place skeleton items in the big cave - remains of previous explorers */
    /* Scale with floor count: 1 skeleton per 40 floor tiles, min 2, max 8 */
    int cave_skeletons = floor_count / 40;
    if (cave_skeletons < 2) cave_skeletons = 2;
    if (cave_skeletons > 8) cave_skeletons = 8;
    int skeletons_placed = 0;
    
    for (int sk = 0; sk < cave_skeletons; ++sk)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(min_y, max_y);
            int sx = rand_range(min_x, max_x);
            if (!in_bounds_fully(sy, sx)) continue;
            if (!cave_floor_bold(sy, sx)) continue;
            if (cave_o_idx[sy][sx] != 0) continue;  /* Already has object */
            
            /* Create and place a skeleton */
            object_type object_type_body;
            object_type *i_ptr = &object_type_body;
            object_wipe(i_ptr);
            
            /* Mix of human/elf/orc skeletons: balanced distribution for caves */
            s16b k_idx;
            {
                int roll = rand_int(100); /* 0-99 */
                if (roll < 35) /* 35% */
                    k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
                else if (roll < 70) /* 35% */
                    k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);
                else /* 30% */
                    k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);
            }
            
            object_prep(i_ptr, k_idx);
            i_ptr->pval = 1;  /* Skeleton level */
            
            drop_near(i_ptr, -1, sy, sx);
            skeletons_placed++;
            break;
        }
    }
    
    log_trace("Big cave anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) edge=%d floors=%d pillars=%d monsters=%d skeletons=%d",
        min_y, min_x, max_y, max_x, cy, cx, found_edge, floor_count, pillar_count, monsters_placed, skeletons_placed);
    genlog_anchor("BIG_CAVE: bounds=(%d,%d)-(%d,%d), %d floor tiles, %d pillars, %d monsters, %d skeletons",
        min_y, min_x, max_y, max_x, floor_count, pillar_count, monsters_placed, skeletons_placed);
    return true;
}

/* Carve a chasm area with organic cave shape and islands connected by bridges */
static bool carve_chasm_with_bridges(int y_min, int y_max, int x_min, int x_max,
    int floor_style, int bridge_style)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("CHASM: rejected - room capacity limit reached");
        return false;
    }
    
    int avail_h = y_max - y_min;
    int avail_w = x_max - x_min;
    if (avail_h < 16 || avail_w < 20)
    {
        genlog_anchor("CHASM: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }
    
    /* Use variable margins to create organic outer boundary */
    int h = avail_h;
    int w = avail_w;
    int y1 = y_min;
    int x1 = x_min;
    int y2 = y_max;
    int x2 = x_max;
    
    /* Check area is basic granite */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("CHASM: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }
    
    /* 
     * CHASM GENERATION APPROACH:
     * 1. Use CA to create organic cave boundary (not rectangular)
     * 2. Create multiple platform islands within the cave
     * 3. Fill non-platform areas with chasms
     * 4. Connect platforms with narrow bridges
     */
    
    /* Track what's inside the cave vs wall, and what's platform vs chasm */
    bool* is_cave = mem_alloc_array(h * w, bool);
    bool* is_platform = mem_alloc_array(h * w, bool);
    if (!is_cave || !is_platform) 
    {
        if (is_cave) mem_free(is_cave);
        if (is_platform) mem_free(is_platform);
        return false;
    }
    
    /* Initialize: seed cave shape with multi-center distance + noise */
    int num_cave_centers = 3 + rand_int(3);  /* 3-5 centers for cave shape */
    int cave_cy[6], cave_cx[6];
    for (int c = 0; c < num_cave_centers; ++c)
    {
        cave_cy[c] = rand_range(h / 4, 3 * h / 4);
        cave_cx[c] = rand_range(w / 4, 3 * w / 4);
    }
    
    /* Carve organic cave shape using distance from centers + noise */
    int base_radius = (h + w) / 5;
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            /* Find distance to nearest center */
            int min_dist = 9999;
            for (int c = 0; c < num_cave_centers; ++c)
            {
                int dy = ABS(ly - cave_cy[c]);
                int dx = ABS(lx - cave_cx[c]);
                int dist = dy + (dx * 2 / 3);  /* Wider horizontally */
                if (dist < min_dist) min_dist = dist;
            }
            
            /* Cave extends with noise for organic edges */
            int threshold = base_radius + rand_int(base_radius / 2) - rand_int(base_radius / 3);
            is_cave[ly * w + lx] = (min_dist < threshold);
            is_platform[ly * w + lx] = false;
        }
    }
    
    /* CA smoothing for organic cave boundary */
    bool* next_cave = mem_alloc_array(h * w, bool);
    if (!next_cave) { mem_free(is_cave); mem_free(is_platform); return false; }
    
    for (int step = 0; step < 3; ++step)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = ly + dy, nx = lx + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors += 0;  /* Edges are wall */
                        else if (is_cave[ny * w + nx])
                            neighbors++;
                    }
                }
                /* Cave survives with 4+ neighbors, born with 5+ */
                next_cave[ly * w + lx] = is_cave[ly * w + lx] ? (neighbors >= 4) : (neighbors >= 5);
            }
        }
        for (int i = 0; i < h * w; ++i) is_cave[i] = next_cave[i];
    }
    mem_free(next_cave);
    
    /* Ensure cave doesn't touch absolute edges */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (ly < 2 || ly >= h - 2 || lx < 2 || lx >= w - 2)
                is_cave[ly * w + lx] = false;
        }
    }
    
    /* Now create 5-9 platform islands within the cave area */
    int num_platforms = rand_range(5, 9);
    int plat_cy[10], plat_cx[10], plat_radius[10];
    int platforms_placed = 0;
    
    for (int attempt = 0; attempt < 300 && platforms_placed < num_platforms; ++attempt)
    {
        int py = rand_range(4, h - 5);
        int px = rand_range(5, w - 6);
        
        /* Must be inside cave */
        if (!is_cave[py * w + px]) continue;
        
        /* Check distance from other platforms */
        bool too_close = false;
        int min_sep = 5 + rand_int(3);  /* Variable separation */
        for (int i = 0; i < platforms_placed; ++i)
        {
            int dist = ABS(py - plat_cy[i]) + ABS(px - plat_cx[i]);
            if (dist < min_sep)
            {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;
        
        plat_cy[platforms_placed] = py;
        plat_cx[platforms_placed] = px;
        plat_radius[platforms_placed] = rand_range(2, 4);
        platforms_placed++;
    }
    
    /* Create organic platform shapes */
    for (int p = 0; p < platforms_placed; ++p)
    {
        int cy = plat_cy[p];
        int cx = plat_cx[p];
        int base_r = plat_radius[p];
        
        for (int ly = 0; ly < h; ++ly)
        {
            for (int lx = 0; lx < w; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;
                
                int dy = ABS(ly - cy);
                int dx = ABS(lx - cx);
                int dist = dy + (dx * 2 / 3);
                
                int threshold = base_r + rand_int(2);
                if (dist <= threshold)
                    is_platform[ly * w + lx] = true;
            }
        }
    }
    
    /* Extend platforms organically */
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int ly = 1; ly < h - 1; ++ly)
        {
            for (int lx = 1; lx < w - 1; ++lx)
            {
                if (!is_cave[ly * w + lx]) continue;
                if (is_platform[ly * w + lx]) continue;
                
                int adj = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dy || dx) && is_platform[(ly+dy) * w + (lx+dx)])
                            adj++;
                
                if (adj >= 3 && one_in_(3))
                    is_platform[ly * w + lx] = true;
            }
        }
    }
    
    /* Add sparse edge nubs instead of a full floor ring to avoid easy perimeter walkways */
    for (int ly = 0; ly < h; ++ly)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!is_cave[ly * w + lx]) continue;

            /* Check if adjacent to non-cave (wall) */
            bool edge_of_cave = false;
            int adj_platforms = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = ly + dy, nx = lx + dx;
                    if (ny < 0 || nx < 0 || ny >= h || nx >= w || !is_cave[ny * w + nx])
                        edge_of_cave = true;
                    else if (is_platform[ny * w + nx])
                        adj_platforms++;
                }
            }
            /* Only add occasional nubs where platforms are already nearby to prevent a continuous ring */
            if (edge_of_cave && !is_platform[ly * w + lx] && adj_platforms >= 2 && one_in_(4))
            {
                is_platform[ly * w + lx] = true;
            }
        }
    }

    /* Apply to cave: inside cave + platform = floor, inside cave + !platform = chasm */
    int floor_count = 0;
    int chasm_count = 0;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            int ly = gy - y1, lx = gx - x1;
            
            if (!is_cave[ly * w + lx])
                continue;  /* Leave as granite wall */
            
            if (is_platform[ly * w + lx])
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, floor_style);
                cave_info[gy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                floor_count++;
            }
            else
            {
                cave_set_feat(gy, gx, FEAT_CHASM);
                cave_info[gy][gx] |= CAVE_CHASM_AREA;
                chasm_count++;
            }
        }
    }
    
    /* Now connect platforms with bridges (MST-style) */
    int global_plat_y[10], global_plat_x[10];
    for (int p = 0; p < platforms_placed; ++p)
    {
        global_plat_y[p] = y1 + plat_cy[p];
        global_plat_x[p] = x1 + plat_cx[p];
    }
    
    bool* connected = mem_alloc_array(platforms_placed, bool);
    if (!connected) { mem_free(is_cave); mem_free(is_platform); return false; }
    for (int i = 0; i < platforms_placed; ++i) connected[i] = false;
    if (platforms_placed > 0) connected[0] = true;
    
    int bridges_built = 0;
    for (int iter = 0; iter < platforms_placed; ++iter)
    {
        int best_from = -1, best_to = -1, best_dist = 9999;
        
        for (int i = 0; i < platforms_placed; ++i)
        {
            if (!connected[i]) continue;
            for (int j = 0; j < platforms_placed; ++j)
            {
                if (connected[j]) continue;
                int dist = distance(global_plat_y[i], global_plat_x[i],
                                   global_plat_y[j], global_plat_x[j]);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best_from = i;
                    best_to = j;
                }
            }
        }
        
        if (best_to < 0) break;
        
        int sy = global_plat_y[best_from];
        int sx = global_plat_x[best_from];
        int ey = global_plat_y[best_to];
        int ex = global_plat_x[best_to];
        
        /* L-shaped bridge */
        if (one_in_(2))
        {
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(sy, gx) && cave_feat[sy][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(sy, gx, FEAT_FLOOR, bridge_style);
                    cave_info[sy][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, ex) && cave_feat[gy][ex] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, ex, FEAT_FLOOR, bridge_style);
                    cave_info[gy][ex] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }
        else
        {
            int y_lo = MIN(sy, ey), y_hi = MAX(sy, ey);
            for (int gy = y_lo; gy <= y_hi; ++gy)
                if (in_bounds_fully(gy, sx) && cave_feat[gy][sx] == FEAT_CHASM)
                {
                    cave_set_feat_style(gy, sx, FEAT_FLOOR, bridge_style);
                    cave_info[gy][sx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
            int x_lo = MIN(sx, ex), x_hi = MAX(sx, ex);
            for (int gx = x_lo; gx <= x_hi; ++gx)
                if (in_bounds_fully(ey, gx) && cave_feat[ey][gx] == FEAT_CHASM)
                {
                    cave_set_feat_style(ey, gx, FEAT_FLOOR, bridge_style);
                    cave_info[ey][gx] |= CAVE_ROOM | CAVE_CHASM_AREA;
                }
        }
        
        connected[best_to] = true;
        bridges_built++;
    }
    
    mem_free(connected);
    mem_free(is_cave);
    mem_free(is_platform);
    
    /* Track bounds of just the floor tiles (not chasm) for proper tunnel connectivity */
    int floor_min_y = y2, floor_max_y = y1, floor_min_x = x2, floor_max_x = x1;
    for (int gy = y1; gy <= y2; ++gy)
    {
        for (int gx = x1; gx <= x2; ++gx)
        {
            if (cave_floor_bold(gy, gx))
            {
                if (gy < floor_min_y) floor_min_y = gy;
                if (gy > floor_max_y) floor_max_y = gy;
                if (gx < floor_min_x) floor_min_x = gx;
                if (gx > floor_max_x) floor_max_x = gx;
            }
        }
    }
    
    /* Set outer walls ONLY around floor tiles (not chasm) for proper tunnel connectivity */
    for (int gy = floor_min_y - 1; gy <= floor_max_y + 1; ++gy)
    {
        for (int gx = floor_min_x - 1; gx <= floor_max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            if (cave_feat[gy][gx] == FEAT_CHASM) continue;  /* Don't convert chasm */
            if (cave_feat[gy][gx] != FEAT_WALL_EXTRA) continue;
            
            /* Only set outer wall if bordering actual floor (not chasm) */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx) &&
                        (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, floor_style);
        }
    }
    
    /* Find center on a floor tile near an outer wall (better for tunnel connectivity) */
    int cy = (floor_min_y + floor_max_y) / 2;
    int cx = (floor_min_x + floor_max_x) / 2;
    
    /* First try: find floor tile adjacent to outer wall */
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(floor_min_y, floor_max_y);
        int tx = rand_range(floor_min_x, floor_max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        /* Check if adjacent to outer wall */
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    cy = ty; cx = tx;
                    found_edge = true;
                }
            }
        }
    }
    
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(floor_min_y, floor_max_y);
            int tx = rand_range(floor_min_x, floor_max_x);
            if (cave_floor_bold(ty, tx))
            {
                cy = ty; cx = tx;
                break;
            }
        }
    }
    
    int idx = dun->cent_n++;
    dun->cent[idx].y = cy;
    dun->cent[idx].x = cx;
    /* Use floor bounds, not full chasm bounds, for tunnel connectivity */
    dun->corner[idx].y1 = floor_min_y;
    dun->corner[idx].x1 = floor_min_x;
    dun->corner[idx].y2 = floor_max_y;
    dun->corner[idx].x2 = floor_max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_CA_BLOB, false);
    
    /* === CHASM MONSTER SPAWNING === */
    /* Place monsters on the platforms - count floor tiles first */
    int chasm_floor_count = 0;
    for (int gy = floor_min_y; gy <= floor_max_y; ++gy)
    {
        for (int gx = floor_min_x; gx <= floor_max_x; ++gx)
        {
            if (in_bounds_fully(gy, gx) && cave_floor_bold(gy, gx))
                chasm_floor_count++;
        }
    }
    
    /* Approximately 1 monster per 35 floor tiles (even more) */
    int chasm_monsters = chasm_floor_count / 35;
    if (chasm_monsters < 10) chasm_monsters = 10;
    if (chasm_monsters > 45) chasm_monsters = 45;
    int monsters_placed = 0;
    
    for (int m = 0; m < chasm_monsters; ++m)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int my = rand_range(floor_min_y, floor_max_y);
            int mx = rand_range(floor_min_x, floor_max_x);
            if (!in_bounds_fully(my, mx)) continue;
            if (!cave_floor_bold(my, mx)) continue;
            if (cave_m_idx[my][mx] != 0) continue;  /* Already has monster */
            if (cave_o_idx[my][mx] != 0) continue;  /* Has object */
            
            /* Prefer shadow creatures in chasms. */
            bool placed = false;
            if (rand_int(100) < 55)
                placed = place_monster_by_letter_try(my, mx, 'w', true, p_ptr->depth);
            if (!placed)
                placed = place_monster(my, mx, true, false, false);
            if (placed)
            {
                monsters_placed++;
                break;
            }
        }
    }
    
    log_trace("Chasm organic: %d platforms, %d bridges, %d chasm tiles, floor=(%d,%d)-(%d,%d) center=(%d,%d) monsters=%d",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x, cy, cx, monsters_placed);
    genlog_anchor("CHASM: %d platforms, %d bridges, %d chasm tiles at (%d,%d)-(%d,%d), %d monsters",
        platforms_placed, bridges_built, chasm_count, floor_min_y, floor_min_x, floor_max_y, floor_max_x, monsters_placed);
    return true;
}

/* Carve a labyrinth-style maze with organic shape using cellular automata */
static bool carve_labyrinth_bounds(int y_min, int y_max, int x_min, int x_max,
    density_level_t density, int style_idx)
{
    if (dun->cent_n >= room_capacity_limit())
    {
        genlog_anchor("LABYRINTH: rejected - room capacity limit reached");
        return false;
    }
    
    int avail_h = y_max - y_min;
    int avail_w = x_max - x_min;
    if (avail_h < 10 || avail_w < 12)
    {
        genlog_anchor("LABYRINTH: rejected - bounds too small (%d,%d)-(%d,%d), avail=%dx%d",
                      y_min, x_min, y_max, x_max, avail_h, avail_w);
        return false;
    }
    
    /* Use small margins to maximize labyrinth size while avoiding partition overlap */
    int margin_y = rand_range(3, 5);
    int margin_x = rand_range(3, 5);
    int y1 = y_min + margin_y;
    int x1 = x_min + margin_x;
    int y2 = y_max - margin_y;
    int x2 = x_max - margin_x;
    int h = y2 - y1 + 1;
    int w = x2 - x1 + 1;
    
    if (h < 8 || w < 10)
    {
        genlog_anchor("LABYRINTH: rejected - after margins too small: h=%d w=%d (margins y=%d x=%d)",
                      h, w, margin_y, margin_x);
        return false;
    }
    
    /* Check area is basic granite - if floor exists, another partition already carved here */
    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (in_bounds_fully(y, x) && cave_floor_bold(y, x))
            {
                genlog_anchor("LABYRINTH: rejected - floor already exists at (%d,%d) in bounds (%d,%d)-(%d,%d)",
                              y, x, y1, x1, y2, x2);
                return false;
            }
        }
    }
    
    /* Use CA to create organic boundary mask - no size caps, use full partition */
    /* Note: h and w already set from margins above, keep them as-is */
    
    bool* mask = mem_alloc_array(h * w, bool);
    if (!mask) return false;
    
    /* Seed with 60% fill for corridors */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            mask[y * w + x] = (rand_int(100) < 60);
    
    /* CA smoothing to create organic boundary */
    bool* next = mem_alloc_array(h * w, bool);
    if (!next) { mem_free(mask); return false; }
    
    for (int step = 0; step < 3; ++step)
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                            neighbors++;
                        else if (mask[ny * w + nx])
                            neighbors++;
                    }
                next[y * w + x] = (neighbors >= 4);
            }
        }
        for (int i = 0; i < h * w; ++i) mask[i] = next[i];
    }
    mem_free(next);
    
    /* Carve corridors in a grid pattern, but only within the organic mask */
    int floor_count = 0;
    int min_y = y2, max_y = y1, min_x = x2, max_x = x1;
    /* Vary corridor spacing by density: sparse=4 (open), normal=3, dense=2 (tight maze) */
    int corridor_spacing = (density == DENSITY_SPARSE) ? 4 : (density == DENSITY_DENSE) ? 2 : 3;
    
    /* Horizontal corridors */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 0; lx < w; ++lx)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    /* Vertical corridors */
    for (int lx = 1; lx < w - 1; lx += corridor_spacing)
    {
        for (int ly = 0; ly < h; ++ly)
        {
            if (!mask[ly * w + lx]) continue;
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (in_bounds_fully(gy, gx) && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat_style(gy, gx, FEAT_FLOOR, style_idx);
                cave_info[gy][gx] |= CAVE_ROOM;
                floor_count++;
                if (gy < min_y) min_y = gy;
                if (gy > max_y) max_y = gy;
                if (gx < min_x) min_x = gx;
                if (gx > max_x) max_x = gx;
            }
        }
    }
    
    mem_free(mask);
    
    /* Block some corridor segments to create dead ends */
    for (int ly = 1; ly < h - 1; ly += corridor_spacing)
    {
        for (int lx = 1; lx < w - 1; lx += corridor_spacing)
        {
            int gy = y1 + ly;
            int gx = x1 + lx;
            if (!in_bounds_fully(gy, gx) || !cave_floor_bold(gy, gx))
                continue;
            
            if (rand_int(100) < 45)
            {
                int block_dir = rand_int(4);
                int dy = (block_dir == 0) ? -1 : (block_dir == 1) ? 1 : 0;
                int dx = (block_dir == 2) ? -1 : (block_dir == 3) ? 1 : 0;
                
                for (int step = 1; step < corridor_spacing; ++step)
                {
                    int ny = gy + dy * step;
                    int nx = gx + dx * step;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx))
                    {
                        cave_set_feat_style(ny, nx, FEAT_WALL_EXTRA, style_idx);
                        cave_info[ny][nx] &= ~CAVE_ROOM;
                        floor_count--;
                    }
                }
            }
        }
    }
    
    /* Add chambers at some intersections */
    int chamber_count = rand_range(2, 5);
    for (int c = 0; c < chamber_count; ++c)
    {
        int cy = rand_range(min_y + 2, max_y - 2);
        int cx = rand_range(min_x + 2, max_x - 2);
        if (!cave_floor_bold(cy, cx)) continue;
        
        int ch_h = rand_range(2, 4);
        int ch_w = rand_range(2, 5);
        
        for (int dy = -ch_h; dy <= ch_h; ++dy)
        {
            for (int dx = -ch_w; dx <= ch_w; ++dx)
            {
                int ty = cy + dy;
                int tx = cx + dx;
                if (!in_bounds_fully(ty, tx)) continue;
                if (cave_feat[ty][tx] != FEAT_WALL_EXTRA) continue;
                
                cave_set_feat_style(ty, tx, FEAT_FLOOR, style_idx);
                cave_info[ty][tx] |= CAVE_ROOM;
                floor_count++;
                if (ty < min_y) min_y = ty;
                if (ty > max_y) max_y = ty;
                if (tx < min_x) min_x = tx;
                if (tx > max_x) max_x = tx;
            }
        }
    }
    
    if (floor_count < 25)
        return false;
    
    /* Set outer walls */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx)) continue;
            if (cave_floor_bold(gy, gx)) continue;
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0) continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                        borders_floor = true;
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
                cave_set_feat_style(gy, gx, FEAT_WALL_OUTER, style_idx);
        }
    }
    
    /* Pick center - prefer floor tile adjacent to outer wall for tunnel connectivity */
    int center_y = (min_y + max_y) / 2, center_x = (min_x + max_x) / 2;
    bool found_edge = false;
    for (int tries = 0; tries < 200 && !found_edge; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (!cave_floor_bold(ty, tx)) continue;
        
        for (int dy = -1; dy <= 1 && !found_edge; ++dy)
        {
            for (int dx = -1; dx <= 1 && !found_edge; ++dx)
            {
                if (dy == 0 && dx == 0) continue;
                if (in_bounds_fully(ty + dy, tx + dx) && 
                    cave_feat[ty + dy][tx + dx] == FEAT_WALL_OUTER)
                {
                    center_y = ty; center_x = tx;
                    found_edge = true;
                }
            }
        }
    }
    /* Fallback: any floor tile */
    if (!found_edge)
    {
        for (int tries = 0; tries < 100; ++tries)
        {
            int ty = rand_range(min_y, max_y);
            int tx = rand_range(min_x, max_x);
            if (cave_floor_bold(ty, tx))
            {
                center_y = ty; center_x = tx; break;
            }
        }
    }
    
    int idx = dun->cent_n++;
    dun->cent[idx].y = center_y;
    dun->cent[idx].x = center_x;
    dun->corner[idx].y1 = min_y;
    dun->corner[idx].x1 = min_x;
    dun->corner[idx].y2 = max_y;
    dun->corner[idx].x2 = max_x;
    dun->kind[idx] = ROOM_KIND_CLASSIC;
    dun->is_quest[idx] = false;
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, false);
    
    /* === LABYRINTH MONSTER SPAWNING === */
    /* Place monsters directly inside the labyrinth - scale with floor count */
    /* Approximately 1 monster per 6 floor tiles (drastically increased) */
    int lab_monsters = floor_count / 6;
    if (lab_monsters < 8) lab_monsters = 8;
    if (lab_monsters > 50) lab_monsters = 50;
    int monsters_placed = 0;
    
    for (int m = 0; m < lab_monsters; ++m)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int my = rand_range(min_y, max_y);
            int mx = rand_range(min_x, max_x);
            if (!in_bounds_fully(my, mx)) continue;
            if (!cave_floor_bold(my, mx)) continue;
            if (cave_m_idx[my][mx] != 0) continue;  /* Already has monster */
            if (cave_o_idx[my][mx] != 0) continue;  /* Has object */
            
            /* Prefer invisible creatures in labyrinths. */
            bool placed = false;
            if (rand_int(100) < 55)
                placed = place_monster_by_flag_try(my, mx, 2, RF2_INVISIBLE, true, p_ptr->depth);
            if (!placed)
                placed = place_monster(my, mx, true, true, false);
            if (placed)
            {
                monsters_placed++;
                break;
            }
        }
    }
    
    /* === LABYRINTH SKELETON SPAWNING === */
    /* Place skeleton items in the labyrinth - those who got lost before you */
    /* Scale with floor count: 1 skeleton per 25 floor tiles, min 2, max 6 */
    int lab_skeletons = floor_count / 25;
    if (lab_skeletons < 2) lab_skeletons = 2;
    if (lab_skeletons > 6) lab_skeletons = 6;
    int skeletons_placed = 0;
    
    for (int sk = 0; sk < lab_skeletons; ++sk)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(min_y, max_y);
            int sx = rand_range(min_x, max_x);
            if (!in_bounds_fully(sy, sx)) continue;
            if (!cave_floor_bold(sy, sx)) continue;
            if (cave_o_idx[sy][sx] != 0) continue;  /* Already has object */
            
            /* Create and place a skeleton */
            object_type object_type_body;
            object_type *i_ptr = &object_type_body;
            object_wipe(i_ptr);
            
            /* Mix of human/elf/orc skeletons: favor human+elf, orcs rare in labyrinth */
            s16b k_idx;
            {
                int roll = rand_int(100); /* 0-99 */
                if (roll < 30) /* 30% */
                    k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
                else if (roll < 90) /* 60% */
                    k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);
                else /* 10% */
                    k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);
            }
            
            object_prep(i_ptr, k_idx);
            i_ptr->pval = 1;  /* Skeleton level */
            
            drop_near(i_ptr, -1, sy, sx);
            skeletons_placed++;
            break;
        }
    }
    
    /* === LABYRINTH STAIR PLACEMENT === */
    /* Place 1-2 stairs inside the labyrinth for navigation */
    int lab_stairs = 1 + (floor_count > 60 ? 1 : 0);
    int stairs_placed = 0;
    
    for (int s = 0; s < lab_stairs; ++s)
    {
        for (int tries = 0; tries < 50; ++tries)
        {
            int sy = rand_range(min_y, max_y);
            int sx = rand_range(min_x, max_x);
            if (!in_bounds_fully(sy, sx)) continue;
            if (!cave_naked_bold(sy, sx)) continue;
            if (!cave_floor_bold(sy, sx)) continue;
            
            /* Avoid placing next to doors */
            if (cave_feat[sy - 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy + 1][sx] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx - 1] == FEAT_DOOR_HEAD) continue;
            if (cave_feat[sy][sx + 1] == FEAT_DOOR_HEAD) continue;
            
            /* Alternate between up and down stairs */
            int feat = (s % 2 == 0) ? FEAT_MORE : FEAT_LESS;
            
            /* At surface, only down; at Morgoth depth, only up */
            if (p_ptr->depth == 0) feat = FEAT_MORE;
            else if (p_ptr->depth >= MORGOTH_DEPTH) feat = FEAT_LESS;
            
            cave_set_feat(sy, sx, feat);
            stairs_placed++;
            break;
        }
    }
    
    log_trace("Labyrinth anchor (organic): bounds=(%d,%d)-(%d,%d) center=(%d,%d) edge=%d floors=%d chambers=%d monsters=%d skeletons=%d stairs=%d",
        min_y, min_x, max_y, max_x, center_y, center_x, found_edge, floor_count, chamber_count, monsters_placed, skeletons_placed, stairs_placed);
    genlog_anchor("LABYRINTH: bounds=(%d,%d)-(%d,%d), %d floor tiles, %d chambers, %d monsters, %d skeletons, %d stairs",
        min_y, min_x, max_y, max_x, floor_count, chamber_count, monsters_placed, skeletons_placed, stairs_placed);
    return true;
}

#if 0
/* Try to seed a few CA blob anchors in unused granite */
static void seed_ca_blob_anchors(void)
{
    /* Scale CA blobs by map size to add connective floor on big levels */
    int panels = (p_ptr->cur_map_hgt / PANEL_HGT);
    int target = 1 + panels / 3; /* e.g., 9 panels -> 4 blobs */
    if (target > 4) target = 4;
    int placed = 0;
    int max_attempts = target * 8;
    for (int attempt = 0; attempt < max_attempts && placed < target; ++attempt)
    {
        if (carve_ca_blob_anchor())
            placed++;
    }
    log_trace("CA blob seeding complete: placed=%d target=%d attempts=%d", placed, target, max_attempts);
}
#endif

/* Carve a BSP-style sliced region into rooms-like rectangles and register anchor */
#if 0
static bool carve_bsp_slice_anchor(void)
{
    if (dun->cent_n >= room_capacity_limit())
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

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
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
    return true;
}
#endif

static bool carve_bsp_slice_anchor_bounds(int y_min, int y_max, int x_min, int x_max)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y_max - y_min < 8 || x_max - x_min < 10)
        return false;

    int h = rand_range(8, MIN(16, y_max - y_min));
    int w = rand_range(10, MIN(20, x_max - x_min));
    int y1 = rand_range(y_min + 1, y_max - h);
    int x1 = rand_range(x_min + 1, x_max - w);
    int y2 = y1 + h - 1;
    int x2 = x1 + w - 1;

    if (!area_is_basic_granite(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
        return false;

    typedef struct { int y1, x1, y2, x2; } slice_rect;
    slice_rect rects[10];
    int rect_count = 1;
    rects[0] = (slice_rect){y1, x1, y2, x2};

    int splits = rand_range(1, 3);
    for (int s = 0; s < splits && rect_count < 10; ++s)
    {
        int pick = rand_int(rect_count);
        slice_rect r = rects[pick];
        int rw = r.x2 - r.x1 + 1;
        int rh = r.y2 - r.y1 + 1;
        bool vertical = (rw > rh) ? true : (rh > rw ? false : one_in_(2));
        if (vertical && rw > 8)
        {
            int cut = rand_range(r.x1 + rw / 3, r.x2 - rw / 3);
            slice_rect a = {r.y1, r.x1, r.y2, cut};
            slice_rect b = {r.y1, cut + 1, r.y2, r.x2};
            if ((a.x2 - a.x1) >= 4 && (b.x2 - b.x1) >= 4)
            {
                rects[pick] = a;
                rects[rect_count++] = b;
            }
        }
        else if (!vertical && rh > 6)
        {
            int cut = rand_range(r.y1 + rh / 3, r.y2 - rh / 3);
            slice_rect a = {r.y1, r.x1, cut, r.x2};
            slice_rect b = {cut + 1, r.x1, r.y2, r.x2};
            if ((a.y2 - a.y1) >= 3 && (b.y2 - b.y1) >= 3)
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
    if (floor_count < 15)
        return false;

    int cy = min_y, cx = min_x;
    for (int tries = 0; tries < 200; ++tries)
    {
        int ty = rand_range(min_y, max_y);
        int tx = rand_range(min_x, max_x);
        if (cave_floor_bold(ty, tx))
        {
            cy = ty; cx = tx; break;
        }
    }

    /* Set outer walls around floor tiles so tunnels can connect */
    for (int gy = min_y - 1; gy <= max_y + 1; ++gy)
    {
        for (int gx = min_x - 1; gx <= max_x + 1; ++gx)
        {
            if (!in_bounds_fully(gy, gx))
                continue;
            if (cave_floor_bold(gy, gx))
                continue;
            /* Check if this wall borders any floor */
            bool borders_floor = false;
            for (int dy = -1; dy <= 1 && !borders_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = gy + dy, nx = gx + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_floor = true;
                    }
                }
            }
            if (borders_floor && cave_feat[gy][gx] == FEAT_WALL_EXTRA)
            {
                cave_set_feat(gy, gx, FEAT_WALL_OUTER);
            }
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
    mark_room_anchor_meta(idx, LAYOUT_ANCHOR_BSP_SLICE, one_in_(5));
    log_trace("BSP slice (bounded) anchor: bounds=(%d,%d)-(%d,%d) center=(%d,%d) floor=%d rects=%d",
        min_y, min_x, max_y, max_x, cy, cx, floor_count, rect_count);
    return true;
}

#if 0
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
#endif

/* Build a room within explicit bounds */
static bool room_build_in_bounds(int typ, int y1, int y2, int x1, int x2)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;
    if (y2 - y1 < 6 || x2 - x1 < 8)
        return false;

    int y = rand_range(MAX(5, y1 + 3), MIN(p_ptr->cur_map_hgt - 5, y2 - 3));
    int x = rand_range(MAX(5, x1 + 3), MIN(p_ptr->cur_map_wid - 5, x2 - 3));

    switch (typ)
    {
    case 8: return build_type8(y, x);
    case 7: return build_type7(y, x);
    case 6: return build_type6(y, x, false);
    case 2: return build_type2(y, x);
    case 1: return build_type1(y, x);
    default: return false;
    }
}

/* Place rooms in randomized order within a partition */
static void place_rooms_randomized(int y1, int y2, int x1, int x2, int depth,
                                   int t1_count, int t2_count, int t6_count, int t7_count,
                                   int *budget_t6, int *budget_t7, int *budget_t8,
                                   int *used_t6, int *used_t7, int *used_t8)
{
    /* Build an array of all room placements needed */
    int total = t1_count + t2_count + t6_count + t7_count;
    if (total <= 0) return;
    if (total > 50) total = 50;  /* Safety cap */
    
    int room_types[50];
    int idx = 0;
    for (int i = 0; i < t1_count && idx < 50; ++i) room_types[idx++] = 1;
    for (int i = 0; i < t2_count && idx < 50; ++i) room_types[idx++] = 2;
    for (int i = 0; i < t6_count && idx < 50; ++i) room_types[idx++] = 6;
    for (int i = 0; i < t7_count && idx < 50; ++i) room_types[idx++] = 7;
    
    /* Fisher-Yates shuffle */
    for (int i = total - 1; i > 0; --i)
    {
        int j = rand_int(i + 1);
        int temp = room_types[i];
        room_types[i] = room_types[j];
        room_types[j] = temp;
    }
    
    /* Place rooms in shuffled order */
    for (int i = 0; i < total; ++i)
    {
        int typ = room_types[i];
        int priority = (typ >= 6) ? 3 : 2;
        place_room_with_budget(typ, y1, y2, x1, x2, priority, depth,
                               budget_t6, budget_t7, budget_t8,
                               used_t6, used_t7, used_t8);
    }
}

/* Smallest depth at which a non-quest greater vault can appear */
static int min_nonquest_gv_depth(void)
{
    static int cached_min_depth = -1;
    if (cached_min_depth >= 0)
        return cached_min_depth;

    int min_depth = 127; /* high sentinel */
    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8)
            continue;
        if (v_ptr->flags & VLT_QUEST)
            continue;
        if (v_ptr->depth < min_depth)
            min_depth = v_ptr->depth;
    }

    /* Fallback to old gating depth if no candidates are present */
    if (min_depth == 127)
        min_depth = 15;

    cached_min_depth = min_depth;
    return cached_min_depth;
}

/* Roll whether this level should reserve a greater vault slot based on vault rarities */
static bool gv_level_roll_allows(int depth, int *out_candidates)
{
    int candidate_count = 0;
    bool passed = false;

    for (int i = 0; i < z_info->v_max; ++i)
    {
        vault_type *v_ptr = &v_info[i];
        if (v_ptr->typ != 8) continue;
        if (v_ptr->flags & VLT_QUEST) continue;
        if (v_ptr->depth > depth) continue;
        if (v_ptr->max_depth != 0 && depth > v_ptr->max_depth) continue;

        /* Skip already-used greater vaults to mirror build_type8 checks */
        bool repeated = false;
        for (int j = 0; j < MAX_GREATER_VAULTS; ++j)
        {
            if (p_ptr->greater_vaults[j] == i)
            {
                repeated = true;
                break;
            }
        }
        if (repeated) continue;

        candidate_count++;
        if (!passed && one_in_(v_ptr->rarity))
        {
            passed = true;
        }
    }

    if (out_candidates) *out_candidates = candidate_count;

    if (candidate_count == 0)
    {
        genlog_partition("GV roll: depth=%d -> no eligible type8 templates (used or quest-only)", depth);
        return false;
    }

    if (passed)
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> PASS (reserve GV this level)", depth, candidate_count);
    }
    else
    {
        genlog_partition("GV roll: depth=%d candidates=%d -> FAIL (no GV this level)", depth, candidate_count);
    }

    return passed;
}

/* Check whether a partition is fully interior (no map-border contact) */
static bool partition_is_interior(int row, int col, int rows, int cols)
{
    return (row > 0) && (row < rows - 1) && (col > 0) && (col < cols - 1);
}

/* Pick the partition whose centre is closest to the map centre, preferring interior slots */
static int choose_central_partition_index(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return -1;

    int best_idx = -1;
    int best_score = 1 << 30;
    int map_cy = p_ptr->cur_map_hgt / 2;
    int map_cx = p_ptr->cur_map_wid / 2;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int pi = row * cols + col;
            int y1, y2, x1, x2;
            if (!compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
                continue;

            int cy = (y1 + y2) / 2;
            int cx = (x1 + x2) / 2;
            int dist = distance(map_cy, map_cx, cy, cx);
            int penalty = partition_is_interior(row, col, rows, cols) ? 0 : 10000;
            int score = dist + penalty;

            if (score < best_score)
            {
                best_score = score;
                best_idx = pi;
            }
        }
    }

    return best_idx;
}

/* Try to drop a greater vault inside the provided partition bounds */
static bool place_gv_in_partition(int y1, int y2, int x1, int x2, int *budget_t8, int *used_t8)
{
    if (!budget_t8 || *budget_t8 <= 0)
        return false;

    /* Can only have one greater vault per level */
    if (g_vault_name[0] != '\0')
        return false;

    bool placed = false;
    for (int attempt = 0; attempt < 3 && !placed; ++attempt)
    {
        placed = room_build_in_bounds(8, y1, y2, x1, x2);
    }

    if (placed)
    {
        (*budget_t8)--;
        if (used_t8)
            (*used_t8)++;
    }

    return placed;
}

/* Place a chest in a random floor location within partition bounds */
static drop_profile drop_profile_for_mode(quadrant_mode_t mode);
static void place_chest_in_partition(
    int y1, int y2, int x1, int x2, bool force_large, quadrant_mode_t mode)
{
    int attempts = 0;
    int max_attempts = 100;
    
    /* Set chest generation context based on mode and parameters */
    /* Mode-specific size distribution: labyrinth = 70/30, others = 50/50 */
    if (mode == QUAD_MODE_LABYRINTH)
        drop_set_chest_mode(1);  /* 70% small, 30% large */
    else
        drop_set_chest_mode(0);  /* 50/50 default */
    
    /* Vault type is 0 for partitions (default 50/35/15 distribution) */
    drop_set_chest_vault_type(0);
    
    while (attempts < max_attempts)
    {
        int cy = rand_range(y1 + 1, y2 - 1);
        int cx = rand_range(x1 + 1, x2 - 1);
        
        if (!in_bounds_fully(cy, cx))
        {
            attempts++;
            continue;
        }
        
        /* Must be floor (not chasm), not occupied, and not in a vault */
        if (cave_empty_bold(cy, cx) && !cave_o_idx[cy][cx] && 
            !(cave_info[cy][cx] & CAVE_G_VAULT))
        {
            object_type object_type_body;
            object_type* i_ptr = &object_type_body;
            object_wipe(i_ptr);

            int depth = p_ptr->depth;
            drop_profile active_profile = drop_profile_for_mode(mode);
            if (!drop_generate_object_profiled(
                    depth, DROP_QUALITY_NORMAL, DROP_TYPE_CHEST, 0, false,
                    &active_profile, i_ptr))
            {
                attempts++;
                continue;
            }

            if (i_ptr->tval == TV_CHEST)
                i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(cy, cx));

            /* Force large chest if requested (for big caves) */
            if (force_large && i_ptr->tval == TV_CHEST)
            {
                /* Force large chest variant (preserve material) */
                int target_sval = -1;
                if (i_ptr->sval == SV_CHEST_SMALL_WOODEN)
                    target_sval = SV_CHEST_LARGE_WOODEN;
                else if (i_ptr->sval == SV_CHEST_SMALL_STEEL)
                    target_sval = SV_CHEST_LARGE_STEEL;
                else if (i_ptr->sval == SV_CHEST_SMALL_JEWELLED)
                    target_sval = SV_CHEST_LARGE_JEWELLED;
                
                if (target_sval > 0)
                {
                    int k_idx = lookup_kind(TV_CHEST, target_sval);
                    if (k_idx)
                    {
                        s16b old_pval = i_ptr->pval;
                        byte old_xtra1 = i_ptr->xtra1;
                        object_prep(i_ptr, k_idx);
                        i_ptr->pval = old_pval;
                        i_ptr->xtra1 = old_xtra1;
                        apply_autoinscription(i_ptr);
                    }
                }
            }

            if (!floor_carry(cy, cx, i_ptr))
            {
                a_info[i_ptr->name1].cur_num = 0;
            }
            genlog_anchor("Placed chest in partition at (%d,%d)", cy, cx);
            return;
        }
        
        attempts++;
    }
    
    genlog_anchor("Failed to place chest in partition after %d attempts", max_attempts);
}

/* Dynamic partition-based generation mix */
static void apply_quadrant_generation_modes(void)
{
    /* Determine partition grid based on level size (in blocks) */
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;  /* Square levels, so hgt == wid */
    int partition_count;
    int grid_rows, grid_cols;
    int depth = p_ptr->depth;
    
    /* Partition scaling - REDUCED partition counts for larger anchors.
     * Each partition should be at least ~40 tiles per side to fit big caves/chasms.
     * 
      * Target partition size: 40-50 tiles per side for optimal anchor fitting.
      * 
      * Scaling by level size:
      *  6 blocks  ( 66x66)  -> 2x2 grid  (4 partitions)  = 33x33 per partition
      *  7 blocks  ( 77x77)  -> 2x2 grid  (4 partitions)  = 38x38 per partition
      *  8 blocks  ( 88x88)  -> 2x2 grid  (4 partitions)  = 44x44 per partition
      *  9 blocks  ( 99x99)  -> 2x2 grid  (4 partitions)  = 49x49 per partition
      * 10 blocks  (110x110) -> 2x3 grid  (6 partitions)  = 55x36 per partition
      * 11 blocks  (121x121) -> 3x3 grid  (9 partitions)  = 40x40 per partition
     * 12 blocks  (132x132) -> 3x3 grid  (9 partitions)  = 44x44 per partition
     * 13 blocks  (143x143) -> 3x3 grid  (9 partitions)  = 47x47 per partition
     * 14 blocks  (154x154) -> 3x4 grid (12 partitions)  = 51x38 per partition
     * 15 blocks  (165x165) -> 4x4 grid (16 partitions)  = 41x41 per partition
     * 16 blocks  (176x176) -> 4x4 grid (16 partitions)  = 44x44 per partition
     * 17 blocks  (187x187) -> 5x4 grid (20 partitions)  = 46x46 per partition
     * 18 blocks  (198x198) -> 5x4 grid (20 partitions)  = 49x49 per partition
     * 19 blocks  (209x209) -> 5x4 grid (20 partitions)  = 52x52 per partition
     * 20 blocks  (220x220) -> 5x4 grid (20 partitions)  = 55x55 per partition
     * 21 blocks  (231x231) -> 5x5 grid (25 partitions)  = 46x46 per partition
     */
    if (blocks <= 9)
    {
        partition_count = 4;
        grid_rows = 2; grid_cols = 2;
    }
    else if (blocks == 10)
    {
        partition_count = 6;
        if (one_in_(2)) { grid_rows = 3; grid_cols = 2; }
        else { grid_rows = 2; grid_cols = 3; }
    }
    else if (blocks <= 13)
    {
        partition_count = 9;
        grid_rows = 3; grid_cols = 3;
    }
    else if (blocks == 14)
    {
        partition_count = 12;
        if (one_in_(2)) { grid_rows = 3; grid_cols = 4; }
        else { grid_rows = 4; grid_cols = 3; }
    }
    else if (blocks <= 16)
    {
        partition_count = 16;
        grid_rows = 4; grid_cols = 4;
    }
    else if (blocks <= 20)
    {
        partition_count = 20;
        if (one_in_(2)) { grid_rows = 5; grid_cols = 4; }
        else { grid_rows = 4; grid_cols = 5; }
    }
    else  /* blocks >= 21 */
    {
        partition_count = 25;
        grid_rows = 5; grid_cols = 5;
    }

    remember_partition_grid(grid_rows, grid_cols, partition_count);
    
    log_trace("Level size %d blocks: using %dx%d partition grid (%d zones)", 
              blocks, grid_rows, grid_cols, partition_count);
    
    /* Generation log: partition grid setup */
    genlog_partition("Grid setup: %d blocks -> %dx%d grid (%d partitions), depth=%d",
                     blocks, grid_rows, grid_cols, partition_count, depth);
    
    /* Allocate mode, style, and density arrays - max 25 partitions now */
    quadrant_mode_t modes[25];
    int partition_styles[25];
    int partition_bridge_styles[25];
    big_cave_type_t partition_big_cave_types[25];
    density_level_t densities[25];
    int gv_partition = -1;
    int gv_min_depth = min_nonquest_gv_depth();
    int gv_candidate_count = 0;
    bool gv_level_allowed = (depth >= gv_min_depth) && gv_level_roll_allows(depth, &gv_candidate_count);
    if (!gv_level_allowed && depth < gv_min_depth) {
        genlog_partition("GV roll: depth=%d below minimum %d -> no GV this level", depth, gv_min_depth);
    }
    if (morgoth_level_active) {
        gv_level_allowed = false; /* Morgoth's throne room replaces normal GVs */
        morgoth_partition_index = choose_central_partition_index(grid_rows, grid_cols);
        genlog_partition("Morgoth level: reserving central partition idx=%d (grid %dx%d)", morgoth_partition_index, grid_rows, grid_cols);
    }

    /* Depth-aware vault budgets (soft caps; clamped to remaining capacity) */
    /* BOOSTED: More rooms and vaults per partition for denser levels */
    int budget_t6 = MIN(room_capacity_limit(), MAX(20, partition_count * 3 + depth));
    int budget_t7 = (depth >= 4) ? MIN(room_capacity_limit(), MAX(6, partition_count + depth / 2)) : 0;
    int budget_t8 = gv_level_allowed ? 1 : 0;
    if (morgoth_level_active) {
        budget_t8 = 0;
    }
    int capacity_remaining = room_capacity_limit() - dun->cent_n;
    if (budget_t8 > capacity_remaining)
        budget_t8 = capacity_remaining;

    /* Reserve space for the dedicated GV attempt before scaling other budgets */
    int capacity_for_regular = capacity_remaining - budget_t8;
    if (capacity_for_regular < 0)
        capacity_for_regular = 0;

    int budget_total = budget_t6 + budget_t7;
    if (budget_total > capacity_for_regular && budget_total > 0) {
        /* Scale budgets down to fit remaining slots (GV slot already reserved) */
        budget_t6 = (budget_t6 * capacity_for_regular) / budget_total;
        budget_t7 = (budget_t7 * capacity_for_regular) / budget_total;
        if (budget_t6 + budget_t7 < capacity_for_regular) {
            budget_t6 = MIN(capacity_for_regular, budget_t6 + 1); /* keep at least one */
        }
    } else if (capacity_for_regular == 0) {
        budget_t6 = 0;
        budget_t7 = 0;
    }
    
    int mode_counts[6] = {0};
    /* Guarantee minimum ROOMY and CAVEY partitions based on partition count */
    /* ROOMY provides reliable standard rooms that connect well */
    int guaranteed_roomy = 1 + partition_count / 5;  /* At least 1 ROOMY, +1 per 5 partitions */
    int guaranteed_cavey = partition_count / 8;      /* 0 for small, 1+ for larger */
    
    /* Initialize with guaranteed modes first */
    int idx = 0;
    for (int i = 0; i < guaranteed_roomy && idx < partition_count; ++i, ++idx)
    {
        modes[idx] = QUAD_MODE_ROOMY;
        mode_counts[QUAD_MODE_ROOMY]++;
    }
    for (int i = 0; i < guaranteed_cavey && idx < partition_count; ++i, ++idx)
    {
        modes[idx] = QUAD_MODE_CAVEY;
        mode_counts[QUAD_MODE_CAVEY]++;
    }
    
    /* Fill remaining with random modes */
    for (; idx < partition_count; ++idx)
    {
        int weights[6];
        for (int m = 0; m < 6; ++m)
        {
            weights[m] = mode_weight_for_depth(
                (quadrant_mode_t)m, depth, blocks, mode_counts, partition_count);
        }
        modes[idx] = pick_weighted_mode(weights, N_ELEMENTS(weights));
        mode_counts[modes[idx]]++;
    }
    
    /* Shuffle all partitions */
    for (int i = partition_count - 1; i > 0; --i)
    {
        int j = rand_int(i + 1);
        quadrant_mode_t temp = modes[i];
        modes[i] = modes[j];
        modes[j] = temp;
    }
    
    log_trace("%d-partition level: %d ROOMY + %d CAVEY guaranteed, others randomized",
              partition_count, guaranteed_roomy, guaranteed_cavey);
    
    genlog_partition("Mode guarantees: %d ROOMY + %d CAVEY required, %d random",
                     guaranteed_roomy, guaranteed_cavey, partition_count - guaranteed_roomy - guaranteed_cavey);

    /* Never allow Morgoth's throne-room partition to be a special-mode partition.
     * Otherwise, environmental effects (labyrinth view loss, big cave penalties, etc.)
     * can bleed into the endgame setpiece. */
    if (morgoth_level_active && morgoth_partition_index >= 0 && morgoth_partition_index < partition_count)
    {
        if (modes[morgoth_partition_index] == QUAD_MODE_LABYRINTH
            || modes[morgoth_partition_index] == QUAD_MODE_CHASM
            || modes[morgoth_partition_index] == QUAD_MODE_BIG_CAVE)
        {
            log_trace("Morgoth level: forcing partition %d mode from %d to ROOMY",
                      morgoth_partition_index, (int)modes[morgoth_partition_index]);
        }
        modes[morgoth_partition_index] = QUAD_MODE_ROOMY;
    }
    
    /* Pick a random visual style and density for each partition */
    for (int i = 0; i < partition_count; ++i)
    {
        partition_bridge_styles[i] = -1;
        partition_big_cave_types[i] = BIG_CAVE_NONE;

        switch (modes[i])
        {
        case QUAD_MODE_CAVEY:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
            break;
        case QUAD_MODE_LABYRINTH:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_LABYRINTH);
            break;
        case QUAD_MODE_CHASM:
            partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CHASM_FLOOR);
            partition_bridge_styles[i] = styles_pick_partition_style(depth, PART_STYLE_CHASM_BRIDGE);
            break;
        case QUAD_MODE_BIG_CAVE:
            partition_big_cave_types[i] = big_cave_type_pick_for_depth(depth);
            if (partition_big_cave_types[i] == BIG_CAVE_ICE)
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_ICE);
            else if (partition_big_cave_types[i] == BIG_CAVE_FIRE)
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_FIRE);
            else
                partition_styles[i] = styles_pick_partition_style(depth, PART_STYLE_BIG_CAVE_POIS);
            break;
        case QUAD_MODE_ROOMY:
        case QUAD_MODE_RUINED:
        default:
            partition_styles[i] = styles_pick_random_from_level();
            break;
        }

        /* Fixed density distribution: 30% sparse, 40% normal, 30% dense */
        int sparse_chance = 30;
        int normal_chance = 40;

        int density_roll = rand_int(100);
        if (density_roll < sparse_chance)
            densities[i] = DENSITY_SPARSE;
        else if (density_roll < sparse_chance + normal_chance)
            densities[i] = DENSITY_NORMAL;
        else
            densities[i] = DENSITY_DENSE;
    }

    record_partition_metadata(modes, densities, partition_count);
    for (int i = 0; i < partition_count && i < 25; ++i)
        current_partition_big_cave_types[i] = partition_big_cave_types[i];
    
    /* Pre-roll for a dedicated greater vault partition (must be interior) */
    if (budget_t8 > 0)
    {
        int gv_candidates[25];
        int gv_interior_count = 0;
        int gv_preferred[25];
        int gv_preferred_count = 0;
        for (int row = 0; row < grid_rows; ++row)
        {
            for (int col = 0; col < grid_cols; ++col)
            {
                if (!partition_is_interior(row, col, grid_rows, grid_cols))
                    continue;
                int idx = row * grid_cols + col;
                if (idx >= partition_count || gv_interior_count >= 25)
                    continue;
                gv_candidates[gv_interior_count++] = idx;

                /* Prefer a non-special partition for greater vaults so their setpiece
                 * effects don't overlap with LABYRINTH/CHASM/BIG_CAVE zones. */
                quadrant_mode_t m = modes[idx];
                if (m != QUAD_MODE_LABYRINTH && m != QUAD_MODE_CHASM && m != QUAD_MODE_BIG_CAVE)
                {
                    if (gv_preferred_count < 25)
                        gv_preferred[gv_preferred_count++] = idx;
                }
            }
        }

        if (gv_interior_count > 0)
        {
            bool used_preferred = (gv_preferred_count > 0);
            gv_partition = used_preferred
                ? gv_preferred[rand_int(gv_preferred_count)]
                : gv_candidates[rand_int(gv_interior_count)];
            int gv_row = gv_partition / grid_cols;
            int gv_col = gv_partition % grid_cols;
            log_trace("Greater vault partition: %d interior options (%d preferred) -> reserve partition %d (row=%d col=%d grid %dx%d%s)",
                      gv_interior_count, gv_preferred_count, gv_partition, gv_row, gv_col,
                      grid_rows, grid_cols, used_preferred ? "" : " fallback");
            genlog_partition("GV partition reserved (rarity passed): depth=%d min_depth=%d interior=%d preferred=%d -> (%d,%d) idx=%d grid=%dx%d%s",
                             depth, gv_min_depth, gv_interior_count, gv_preferred_count,
                             gv_row, gv_col, gv_partition, grid_rows, grid_cols,
                             used_preferred ? "" : " fallback");
        }
        else
        {
            log_trace("Greater vault partition: no eligible interior partitions for %dx%d grid",
                      grid_rows, grid_cols);
            genlog_partition("GV partition skipped: no interior partitions for grid %dx%d (depth=%d)", grid_rows, grid_cols, depth);
            gv_partition = -1;
            budget_t8 = 0; /* No dedicated slot this level */
        }
    }
    
    /* Mode name strings for logging */
    const char *mode_str[] = {"ROOMY", "CAVEY", "RUINED", "LABYRINTH", "CHASM", "BIG_CAVE"};
    const char *density_str[] = {"SPARSE", "NORMAL", "DENSE"};
    int used_t6 = 0, used_t7 = 0, used_t8 = 0;
    bool gv_partition_attempted = false;
    int partitions_skipped = 0;
    int skipped_soft_fill = 0;
    int skip_cap = MAX(2, partition_count / 5); /* cap outright skips to keep coverage */

    /* Track which partitions have been processed */
    bool partition_done[25];
    for (int i = 0; i < 25; ++i)
        partition_done[i] = false;

    /* TWO-PASS PROCESSING:
     * Pass 1: Process special modes (LABYRINTH, CHASM, BIG_CAVE) first.
     *         These need clear space for anchor carving, so they must run
     *         before ROOMY/CAVEY can place rooms that encroach on neighbors.
     * Pass 2: Process remaining modes (ROOMY, CAVEY, RUINED).
     */
    genlog_partition("Processing special modes first (LABYRINTH, CHASM, BIG_CAVE) to ensure clear space");
    
    /* Pass 1: Special modes only */
    for (int pi = 0; pi < partition_count; ++pi)
    {
        quadrant_mode_t mode = modes[pi];
        bool is_gv_partition = (pi == gv_partition);
        bool is_morgoth_partition = (morgoth_level_active && pi == morgoth_partition_index);
        bool is_special_mode = (mode == QUAD_MODE_LABYRINTH || mode == QUAD_MODE_CHASM || mode == QUAD_MODE_BIG_CAVE);
        if (!is_gv_partition && !is_special_mode && !is_morgoth_partition)
            continue;  /* Skip non-special modes for now */

        if (dun->cent_n >= room_capacity_limit())
        {
            log_trace("Partition gen: room capacity reached (%d/%d), skipping remaining partitions",
                      dun->cent_n, room_capacity_limit());
            break;
        }

        /* Calculate partition boundaries based on grid */
        int before_cent = dun->cent_n;
        int row = pi / grid_cols;
        int col = pi % grid_cols;
        
        /* Calculate boundaries */
        int y1 = (row * p_ptr->cur_map_hgt / grid_rows);
        int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows);
        int x1 = (col * p_ptr->cur_map_wid / grid_cols);
        int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols);
        
        /* Ensure we don't go out of bounds */
        if (y1 < 1) y1 = 1;
        if (x1 < 1) x1 = 1;
        if (y2 >= p_ptr->cur_map_hgt - 1) y2 = p_ptr->cur_map_hgt - 2;
        if (x2 >= p_ptr->cur_map_wid - 1) x2 = p_ptr->cur_map_wid - 2;
        
        if (is_morgoth_partition)
        {
            morgoth_partition_bounds.y1 = y1;
            morgoth_partition_bounds.y2 = y2;
            morgoth_partition_bounds.x1 = x1;
            morgoth_partition_bounds.x2 = x2;
            morgoth_vault_center_y = (y1 + y2) / 2;
            morgoth_vault_center_x = (x1 + x2) / 2;
            morgoth_partition_reserved = true;
            
            /* Place and seal Morgoth's throne room IMMEDIATELY to prevent other 
             * partitions from placing content in this area. The permanent wall sealing
             * must happen before any other room/corridor generation. */
            vault_type* v_ptr = NULL;
            int cy = morgoth_vault_center_y;
            int cx = morgoth_vault_center_x;
            
            if (build_type9(cy, cx, &v_ptr))
            {
                carve_morgoth_entry_tunnels(v_ptr, cy, cx);
                seal_morgoth_partition(v_ptr, cy, cx);
                partition_done[pi] = true;
                genlog_partition("Morgoth partition placed and sealed at idx=%d bounds=(%d,%d)-(%d,%d) center=(%d,%d)", 
                                pi, y1, x1, y2, x2, cy, cx);
            }
            else
            {
                log_trace("Morgoth level: failed to build throne room at (%d,%d) in partition %d", cy, cx, pi);
                morgoth_partition_reserved = false;  /* Allow fallback */
            }
            continue;
        }

        /* mode already declared at loop start for the continue check */
        int style_idx = partition_styles[pi];
        int bridge_style = partition_bridge_styles[pi];
        big_cave_type_t cave_type = partition_big_cave_types[pi];
        density_level_t density = densities[pi];
        int area = (y2 - y1 + 1) * (x2 - x1 + 1);
        int area_factor = MAX(1, MIN(3, (area + 1100) / 1200));
        int floor_pct = 0, icky_pct = 0;
        bool reserved = area_is_reserved_or_dense(y1, y2, x1, x2, &floor_pct, &icky_pct);

        log_trace("Partition %d [%d,%d] (pass 1%s): mode=%s density=%s bounds=(%d,%d)-(%d,%d) area=%d floor=%d%% icky=%d%%",
                  pi, row, col, is_gv_partition ? " GV" : "", mode_str[mode], density_str[density], y1, x1, y2, x2, area, floor_pct, icky_pct);

        if (reserved && partitions_skipped >= skip_cap) {
            /* Too many skips already: fall back to a light recipe instead of skipping */
            log_trace("Partition %d [%d,%d]: reserved but skip_cap reached; using soft-fill", pi, row, col);
            reserved = false;
            skipped_soft_fill++;
            /* Downgrade density to sparse to reduce conflicts */
            density = DENSITY_SPARSE;
        }

        if (reserved) {
            log_trace("Partition %d [%d,%d]: skipping (reserved/quest/icky overlap)", pi, row, col);
            if (is_gv_partition) {
                gv_partition = -1;
                budget_t8 = 0;
            }
            partitions_skipped++;
            continue;
        }

        if (is_gv_partition)
        {
            gv_partition_attempted = true;
            bool placed_gv = place_gv_in_partition(y1, y2, x1, x2, &budget_t8, &used_t8);
            if (placed_gv)
            {
                log_trace("Partition %d [%d,%d]: placed greater vault within bounds (%d,%d)-(%d,%d)",
                          pi, row, col, y1, x1, y2, x2);
                genlog_partition("GV placed in partition [%d,%d] idx=%d bounds=(%d,%d)-(%d,%d) remaining_t8=%d",
                                 row, col, pi, y1, x1, y2, x2, budget_t8);
                partition_done[pi] = true;
                continue;
            }

            log_trace("Partition %d [%d,%d]: greater vault placement failed, falling back to mode logic",
                      pi, row, col);
            genlog_partition("GV placement failed in partition [%d,%d] idx=%d bounds=(%d,%d)-(%d,%d); disabling GV for this attempt",
                             row, col, pi, y1, x1, y2, x2);
            gv_partition = -1;
            budget_t8 = 0;
            if (!is_special_mode)
                continue;
        }

        /* PARTITION MODE TYPES:
         * - ROOMY: Traditional dungeon - balanced mix of all room types
         * - CAVEY: Natural cave system with CA blobs and minimal rooms
         * - RUINED: Ancient carved BSP passages with rooms
         * - LABYRINTH: Maze corridors with chambers
         * - CHASM: Platforms over chasms connected by bridges
         * - BIG_CAVE: Single massive irregular cavern with rooms inside
         */
        switch (mode)
        {
        case QUAD_MODE_CAVEY:
            {
                /* Natural cave system: CA blobs with quartz veins */
                int area = (y2 - y1) * (x2 - x1);
                int base_blobs = 2 + area / 400;  /* Scale with partition size */
                int blob_target = (density == DENSITY_SPARSE) ? base_blobs : 
                                  (density == DENSITY_DENSE) ? base_blobs + 2 : base_blobs + 1;
                if (blob_target > 6) blob_target = 6;
                int carved_blobs = 0;
                
                for (int b = 0; b < blob_target; ++b)
                    if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, style_idx))
                        carved_blobs++;
                
                /* Scatter quartz veins for natural cave look */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);
                
                /* Scatter gems and mithril in cave areas - normal cave bonus */
                int blob_for_loot = (carved_blobs > 0) ? carved_blobs : blob_target;
                scatter_cave_gems_in_bounds(y1, y2, x1, x2, false, blob_for_loot);
                
                /* Caves with rooms scattered inside */
                /* Sparse: T1=2 T2=1 T6=2 T7=0 | Normal: T1=2 T2=2 T6=2 T7=1 | Dense: T1=2 T2=3 T6=3 T7=1 */
                int std_count = scaled_attempts(2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 1, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        case QUAD_MODE_LABYRINTH:
            {
                /* Maze corridors - oppressive, fewer rooms */
                bool carved = carve_labyrinth_bounds(y1, y2, x1, x2, density, style_idx);
                if (!carved)
                {
                    /* Fallback: more BSP slices for maze-like feel */
                    int maze_count = (density == DENSITY_SPARSE) ? 6 : 
                                     (density == DENSITY_DENSE) ? 12 : 8;
                    for (int b = 0; b < maze_count; ++b)
                        carve_bsp_slice_anchor_bounds(y1, y2, x1, x2);
                    /* Update partition mode to match fallback generation (use RUINED for BSP slices) */
                    current_partition_modes[pi] = QUAD_MODE_RUINED;
                    style_idx = styles_pick_random_from_level();
                    partition_styles[pi] = style_idx;
                }
                
                /* Add some dead-end interest: occasional rubble in corridors */
                for (int gy = y1; gy <= y2; ++gy)
                {
                    for (int gx = x1; gx <= x2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (!cave_floor_bold(gy, gx)) continue;
                        /* Very low rubble chance for claustrophobic feel */
                        if (one_in_(40))
                            cave_set_feat_style(gy, gx, FEAT_RUBBLE, style_idx);
                    }
                }
                
                /* Labyrinth with chambers and vaults */
                /* Sparse: T1=1 T2=0 T6=1 T7=0 | Normal: T1=1 T2=1 T6=1 T7=0 | Dense: T1=1 T2=1 T6=2 T7=1 */
                int std_count = scaled_attempts(1, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : 1, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 0, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
                
                /* Place 1 chest in labyrinth partition ONLY if it actually carved */
                if (carved)
                    place_chest_in_partition(y1, y2, x1, x2, false, mode);
            }
            break;
        case QUAD_MODE_CHASM:
            {
                /* Chasm with platforms connected by bridges - no additional rooms */
                bool chasm_carved = carve_chasm_with_bridges(y1, y2, x1, x2,
                    style_idx, bridge_style);
                if (!chasm_carved)
                {
                    /* Fallback: use CA blobs to keep the open feel */
                    int ca_style = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
                    int blob_count = (density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3;
                    for (int b = 0; b < blob_count; ++b)
                        carve_ca_blob_anchor_bounds(y1, y2, x1, x2, ca_style);
                    /* Update partition mode to match fallback generation */
                    current_partition_modes[pi] = QUAD_MODE_CAVEY;
                    style_idx = ca_style;
                    partition_styles[pi] = ca_style;
                    bridge_style = -1;
                    partition_bridge_styles[pi] = -1;
                }

                /* Veins in chasm walls for mining (tagged for star-iron drops) */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, CAVE_CHASM_AREA);
                /* Scatter rare star-iron pieces onto whatever ground exists */
                scatter_chasm_star_iron_in_bounds(y1, y2, x1, x2);

                /* Place 2 guaranteed chests in chasm partition ONLY if it actually carved */
                if (chasm_carved)
                {
                    place_chest_in_partition(y1, y2, x1, x2, false, mode);
                    place_chest_in_partition(y1, y2, x1, x2, false, mode);
                }
            }
            break;
        case QUAD_MODE_BIG_CAVE:
            {
                /* Single massive cavern - the cave IS the room */
                bool carved = carve_big_cave_bounds(y1, y2, x1, x2, style_idx, cave_type);
                int blob_count = 0;
                int carved_blobs = 0;
                if (!carved)
                {
                    /* Fallback: many overlapping blobs */
                    int ca_style = styles_pick_partition_style(depth, PART_STYLE_CA_BLOB);
                    blob_count = (density == DENSITY_SPARSE) ? 5 : 
                                 (density == DENSITY_DENSE) ? 10 : 7;
                    for (int b = 0; b < blob_count; ++b)
                        if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, ca_style))
                            carved_blobs++;
                    /* Update partition mode to match fallback generation */
                    current_partition_modes[pi] = QUAD_MODE_CAVEY;
                    current_partition_big_cave_types[pi] = BIG_CAVE_NONE;
                    partition_big_cave_types[pi] = BIG_CAVE_NONE;
                    style_idx = ca_style;
                    partition_styles[pi] = ca_style;
                }
                
                /* Add quartz veins for natural cave look */
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);
                if (!carved)
                {
                    int blob_for_loot = (carved_blobs > 0) ? carved_blobs : blob_count;
                    scatter_cave_gems_in_bounds(y1, y2, x1, x2, true, MAX(1, blob_for_loot));
                }
                
                /* Add internal pillars/boulders for visual interest (density-scaled) */
                int pillar_target = (density == DENSITY_SPARSE) ? 3 : 
                                    (density == DENSITY_DENSE) ? 10 : 6;
                int pillars_placed = 0;
                for (int tries = 0; tries < 100 && pillars_placed < pillar_target; ++tries)
                {
                    int py = rand_range(y1 + 3, y2 - 3);
                    int px = rand_range(x1 + 3, x2 - 3);
                    if (!in_bounds_fully(py, px)) continue;
                    if (!cave_floor_bold(py, px)) continue;
                    
                    /* Check all neighbors are floor */
                    bool all_floor = true;
                    for (int dy = -1; dy <= 1 && all_floor; ++dy)
                        for (int dx = -1; dx <= 1 && all_floor; ++dx)
                            if (!cave_floor_bold(py + dy, px + dx))
                                all_floor = false;
                    
                    if (all_floor)
                    {
                        cave_set_feat_style(py, px, FEAT_WALL_EXTRA, style_idx);
                        pillars_placed++;
                    }
                }
                
                /* Add some simple rooms scattered in the big cave */
                /* Sparse: T1=1 T6=1 | Normal: T1=1 T6=1 | Dense: T1=1 T6=2 */
                int std_count = scaled_attempts(1, area_factor);
                int int_count = scaled_attempts((density == DENSITY_DENSE) ? 2 : 1, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, 0, int_count, 0,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);

                /* Guarantee a large chest in big caves (material varies) */
                place_chest_in_partition(y1, y2, x1, x2, true, mode);
            }
            break;
        case QUAD_MODE_ROOMY:
        default:
            {
                /* Traditional dungeon - packed with rooms and vaults */
                /* Sparse: T1=2 T2=1 T6=2 T7=1 | Normal: T1=3 T2=2 T6=3 T7=2 | Dense: T1=4 T2=3 T6=4 T7=3 */
                int std_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        }

        /* Add boundary openings for hard-to-exit modes */
        if (mode == QUAD_MODE_CHASM || mode == QUAD_MODE_BIG_CAVE || mode == QUAD_MODE_LABYRINTH)
        {
            ensure_partition_boundary_openings(y1, y2, x1, x2, 3);
        }

        /* Per-partition fallback: if nothing landed, drop a simple room to avoid voids */
        if (dun->cent_n == before_cent)
        {
            int fallback_style = styles_pick_random_from_level();
            style_idx = fallback_style;
            partition_styles[pi] = fallback_style;
            if (room_build_in_bounds(1, y1, y2, x1, x2) || room_build_in_bounds(2, y1, y2, x1, x2))
                log_trace("Partition %d [%d,%d]: fallback simple room placed", pi, row, col);
        }

        /* Apply the partition's visual style to its granite walls.
         * Use a jagged/organic boundary instead of a straight line. */
        if (style_idx >= 0)
        {
            int blend_zone = 3;

            for (int y = y1; y <= y2; ++y)
            {
                for (int x = x1; x <= x2; ++x)
                {
                    if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                        continue;

                    int dist_top = y - y1;
                    int dist_bot = y2 - y;
                    int dist_left = x - x1;
                    int dist_right = x2 - x;
                    int dist_edge = MIN(MIN(dist_top, dist_bot), MIN(dist_left, dist_right));

                    if (dist_edge >= blend_zone)
                    {
                        cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, style_idx);
                    }
                    else
                    {
                        int chance = 20 + (dist_edge * 67 / blend_zone);
                        if (rand_int(100) < chance)
                        {
                            cave_set_feat_with_color(y, x, FEAT_WALL_EXTRA, style_idx);
                        }
                    }
                }
            }
        }

        /* Mark partition as done */
        partition_done[pi] = true;
    }

    /* Pass 2: Process remaining non-special modes (ROOMY, CAVEY, RUINED) */
    genlog_partition("Pass 2: Processing standard modes (ROOMY, CAVEY, RUINED)");
    for (int pi = 0; pi < partition_count; ++pi)
    {
        if (partition_done[pi])
            continue;  /* Already processed in Pass 1 */

        if (dun->cent_n >= room_capacity_limit())
        {
            log_trace("Partition gen: room capacity reached (%d/%d), skipping remaining partitions",
                      dun->cent_n, room_capacity_limit());
            break;
        }

        /* Calculate partition boundaries based on grid */
        int before_cent = dun->cent_n;
        int row = pi / grid_cols;
        int col = pi % grid_cols;
        
        /* Calculate boundaries */
        int y1 = (row * p_ptr->cur_map_hgt / grid_rows);
        int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows);
        int x1 = (col * p_ptr->cur_map_wid / grid_cols);
        int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols);
        
        /* Ensure we don't go out of bounds */
        if (y1 < 1) y1 = 1;
        if (x1 < 1) x1 = 1;
        if (y2 >= p_ptr->cur_map_hgt - 1) y2 = p_ptr->cur_map_hgt - 2;
        if (x2 >= p_ptr->cur_map_wid - 1) x2 = p_ptr->cur_map_wid - 2;
        
        quadrant_mode_t mode = modes[pi];
        density_level_t density = densities[pi];
        int style_idx = partition_styles[pi];
        int area = (y2 - y1 + 1) * (x2 - x1 + 1);
        int area_factor = MAX(1, MIN(3, (area + 1100) / 1200));
        int floor_pct = 0, icky_pct = 0;
        bool reserved = area_is_reserved_or_dense(y1, y2, x1, x2, &floor_pct, &icky_pct);

        log_trace("Partition %d [%d,%d] (pass 2): mode=%s density=%s bounds=(%d,%d)-(%d,%d)",
                  pi, row, col, mode_str[mode], density_str[density], y1, x1, y2, x2);

        if (reserved && partitions_skipped >= skip_cap) {
            reserved = false;
            skipped_soft_fill++;
            density = DENSITY_SPARSE;
        }

        if (reserved) {
            log_trace("Partition %d [%d,%d]: skipping (reserved/quest/icky overlap)", pi, row, col);
            partitions_skipped++;
            continue;
        }

        /* Process the partition based on its mode (standard modes only here) */
        switch (mode)
        {
        case QUAD_MODE_CAVEY:
            {
                int blob_target = 2 + (y2 - y1) * (x2 - x1) / 400;
                if (blob_target > 6) blob_target = 6;
                int carved_blobs = 0;
                for (int b = 0; b < blob_target; ++b)
                    if (carve_ca_blob_anchor_bounds(y1, y2, x1, x2, style_idx))
                        carved_blobs++;
                scatter_quartz_veins_in_bounds(y1, y2, x1, x2, 0);
                int blob_for_loot = (carved_blobs > 0) ? carved_blobs : blob_target;
                scatter_cave_gems_in_bounds(y1, y2, x1, x2, false, blob_for_loot);
                int std_count = scaled_attempts(2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_DENSE) ? 4 : (density == DENSITY_SPARSE) ? 2 : 3, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : (density == DENSITY_DENSE) ? 1 : 1, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        case QUAD_MODE_RUINED:
            {
                int std_count = scaled_attempts(1, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 0 : 1, area_factor);
                int int_count = scaled_attempts((density == DENSITY_DENSE) ? 2 : 1, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_DENSE) ? 1 : 0, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);

                int carve_count = 3 + (y2 - y1) * (x2 - x1) / 500;
                if (carve_count > 10) carve_count = 10;
                for (int b = 0; b < carve_count; ++b)
                    carve_bsp_slice_anchor_bounds(y1, y2, x1, x2);
                
                /* Add rubble to carved floor tiles (5-10-15% based on density) */
                int rubble_chance = (density == DENSITY_SPARSE) ? 3 : 
                                    (density == DENSITY_DENSE) ? 10 : 7;
                for (int gy = y1; gy <= y2; ++gy)
                {
                    for (int gx = x1; gx <= x2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (!cave_floor_bold(gy, gx)) continue;
                        if (cave_info[gy][gx] & CAVE_G_VAULT) continue; /* preserve greater vaults only */
                        if (rand_int(100) < rubble_chance)
                            cave_set_feat(gy, gx, FEAT_RUBBLE);
                    }
                }
                
                /* Add broken wall segments */
                for (int gy = y1 + 2; gy <= y2 - 2; ++gy)
                {
                    for (int gx = x1 + 2; gx <= x2 - 2; ++gx)
                    {
                        if (!in_bounds_fully(gy, gx)) continue;
                        if (cave_info[gy][gx] & CAVE_G_VAULT) continue; /* preserve greater vaults only */
                        if (cave_feat[gy][gx] != FEAT_WALL_OUTER) continue;
                        if (rand_int(100) < 30)
                        {
                            cave_set_feat(gy, gx, FEAT_FLOOR);
                            cave_info[gy][gx] |= CAVE_ROOM;
                            if (one_in_(2))
                                cave_set_feat(gy, gx, FEAT_RUBBLE);
                        }
                    }
                }
                
                /* === RUINED SKELETON SPAWNING === */
                /* After rubble and rooms are created, spawn skeleton items.
                 * Use floor_count / 15, clamp 3..10 (denser than labyrinth).
                 */
                {
                    int floor_count = 0;
                    for (int gy = y1; gy <= y2; ++gy)
                    {
                        for (int gx = x1; gx <= x2; ++gx)
                        {
                            if (!in_bounds_fully(gy, gx)) continue;
                            if (!cave_floor_bold(gy, gx)) continue;
                            if (cave_info[gy][gx] & CAVE_G_VAULT) continue; /* preserve vaults */
                            floor_count++;
                        }
                    }

                    int ru_skeletons = floor_count / 15;
                    if (ru_skeletons < 3) ru_skeletons = 3;
                    if (ru_skeletons > 10) ru_skeletons = 10;
                    
                    genlog_anchor("RUINED partition bounds=(%d,%d)-(%d,%d) floor_count=%d target_skeletons=%d",
                                  y1, x1, y2, x2, floor_count, ru_skeletons);

                    int skeletons_placed = 0;
                    for (int sk = 0; sk < ru_skeletons; ++sk)
                    {
                        for (int tries = 0; tries < 50; ++tries)
                        {
                            int sy = rand_range(y1, y2);
                            int sx = rand_range(x1, x2);
                            if (!in_bounds_fully(sy, sx)) continue;
                            if (!cave_floor_bold(sy, sx)) continue;
                            /* Don't bury skeletons under rubble */
                            if (cave_feat[sy][sx] == FEAT_RUBBLE) continue;
                            if (cave_o_idx[sy][sx] != 0) continue;  /* Already has object */

                            object_type object_type_body;
                            object_type *i_ptr = &object_type_body;
                            object_wipe(i_ptr);

                            s16b k_idx;
                            int roll = rand_int(100);
                            if (roll < 60) /* 60% orc */
                                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);
                            else if (roll < 80) /* 20% human */
                                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
                            else /* 20% elf */
                                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);

                            object_prep(i_ptr, k_idx);
                            i_ptr->pval = 1;  /* Skeleton level */

                            drop_near(i_ptr, -1, sy, sx);
                            skeletons_placed++;
                            break;
                        }
                    }

                    if (skeletons_placed > 0)
                        genlog_anchor("RUINED partition: placed %d skeletons in bounds (%d,%d)-(%d,%d)",
                                      skeletons_placed, y1, x1, y2, x2);

                    /* Ruined partitions also get a small independent scatter of
                     * damaged gear, so broken items can come from the ruins
                     * themselves and not only from searched skeletons. */
                    {
                        int damaged_target = ru_skeletons / 3;
                        if (damaged_target < 1) damaged_target = 1;
                        if (damaged_target > 3) damaged_target = 3;

                        int damaged_placed = place_ruined_partition_damaged_items(
                            y1, y2, x1, x2, damaged_target);
                        if (damaged_placed > 0)
                            genlog_anchor("RUINED partition: placed %d damaged items in bounds (%d,%d)-(%d,%d)",
                                          damaged_placed, y1, x1, y2, x2);
                    }
                }
            }
            break;
        default:
            {
                /* ROOMY or fallback: Traditional dungeon */
                int std_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int cross_count = scaled_attempts((density == DENSITY_SPARSE) ? 1 : (density == DENSITY_DENSE) ? 3 : 2, area_factor);
                int int_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                int vault_count = scaled_attempts((density == DENSITY_SPARSE) ? 2 : (density == DENSITY_DENSE) ? 4 : 3, area_factor);
                place_rooms_randomized(y1, y2, x1, x2, depth, std_count, cross_count, int_count, vault_count,
                                       &budget_t6, &budget_t7, &budget_t8, &used_t6, &used_t7, &used_t8);
            }
            break;
        }

        /* Per-partition fallback */
        if (dun->cent_n == before_cent)
        {
            if (room_build_in_bounds(1, y1, y2, x1, x2) || room_build_in_bounds(2, y1, y2, x1, x2))
            {
                log_trace("Partition %d [%d,%d]: fallback simple room placed", pi, row, col);
                /* Update partition mode to ROOMY since we fell back to standard rooms */
                current_partition_modes[pi] = QUAD_MODE_ROOMY;
            }
        }
    }

    /* Log partition generation summary */
    log_debug("Generation summary: %d blocks, %dx%d grid (%d partitions), %d rooms created",
              blocks, grid_rows, grid_cols, partition_count, dun->cent_n);
    log_debug("Partition budgets: used t6=%d/t7=%d/t8=%d remaining t6=%d t7=%d t8=%d skipped_parts=%d soft_fill=%d",
              used_t6, used_t7, used_t8, budget_t6, budget_t7, budget_t8, partitions_skipped, skipped_soft_fill);
    log_trace("Greater vault partition summary: attempted=%s placed=%d",
              gv_partition_attempted ? "yes" : "no", used_t8);
    
    /* Detailed generation log summary */
    genlog_summary("Partition phase complete: %d rooms from %d partitions (%d skipped, %d soft-fill skipped)",
                   dun->cent_n, partition_count, partitions_skipped, skipped_soft_fill);
    genlog_summary("Room budgets - T6: %d used / T7: %d used / T8: %d used",
                   used_t6, used_t7, used_t8);
    
    /* Log mode distribution and persist labyrinth count for monster/stair bonuses */
    {
        int mode_counts_summary[6] = {0};
        for (int mi = 0; mi < partition_count; ++mi)
            mode_counts_summary[modes[mi]]++;
        current_labyrinth_partitions = mode_counts_summary[QUAD_MODE_LABYRINTH];
        genlog_partition("Mode distribution: ROOMY=%d CAVEY=%d RUINED=%d LABYRINTH=%d CHASM=%d BIG_CAVE=%d",
                         mode_counts_summary[0], mode_counts_summary[1], mode_counts_summary[2],
                         mode_counts_summary[3], mode_counts_summary[4], mode_counts_summary[5]);
    }
    
    if (op_ptr && show_level_generation_debug)
    {
        msg_format("Gen: %d blocks, %dx%d grid (%d parts), %d rooms",
                   blocks, grid_rows, grid_cols, partition_count, dun->cent_n);

        /* Build a summary of partition modes for the optional on-screen debug output. */
        int mode_counts_debug[6] = {0};
        for (int i = 0; i < partition_count; ++i)
            mode_counts_debug[modes[i]]++;

        msg_format("Modes: R:%d C:%d U:%d L:%d H:%d B:%d",
                   mode_counts_debug[0], mode_counts_debug[1], mode_counts_debug[2],
                   mode_counts_debug[3], mode_counts_debug[4], mode_counts_debug[5]);
    }
}

/* Carve connection corridors at partition boundaries to ensure inter-partition connectivity.
 * This helps when caves/labyrinths in adjacent partitions don't naturally connect.
 * IMPROVED: Now searches deeper into partitions (15 tiles) and carves longer corridors (8 tiles).
 * Also tries multiple x/y positions per boundary segment. */
#if 0
static void ensure_partition_connectivity(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int grid_rows = current_partition_rows;
    int grid_cols = current_partition_cols;
    
    /* Reuse the grid chosen during generation; fall back if unavailable */
    if (grid_rows <= 0 || grid_cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &grid_rows, &grid_cols);
    }
    
    int connections_added = 0;
    const int SEARCH_DEPTH = 15;  /* How far into partition to look for floor (was 5) */
    const int CORRIDOR_LEN = 8;   /* How long the carved corridor is (was 3) */
    const int ATTEMPTS_PER_SEGMENT = 3;  /* Try multiple positions per boundary segment */
    
    genlog_connect("ensure_partition_connectivity: %dx%d grid, searching %d deep, carving %d long",
                   grid_rows, grid_cols, SEARCH_DEPTH, CORRIDOR_LEN);
    
    /* Create horizontal boundary connections (between rows) */
    for (int row = 0; row < grid_rows - 1; ++row)
    {
        int boundary_y = ((row + 1) * p_ptr->cur_map_hgt / grid_rows);
        
        for (int col = 0; col < grid_cols; ++col)
        {
            int x1 = (col * p_ptr->cur_map_wid / grid_cols) + 2;
            int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 2;
            
            /* Try multiple x positions for better coverage */
            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cx = rand_range(x1 + 2, x2 - 2);
                
                /* Find nearest floor above and below the boundary */
                int floor_above_y = -1, floor_above_x = -1;
                int floor_below_y = -1, floor_below_x = -1;
                
                for (int dx = -5; dx <= 5; ++dx)
                {
                    int tx = cx + dx;
                    if (tx < 1 || tx >= p_ptr->cur_map_wid - 1) continue;
                    
                    for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
                    {
                        if (floor_above_y < 0 && in_bounds_fully(boundary_y - dy, tx) 
                            && cave_floor_bold(boundary_y - dy, tx))
                        {
                            floor_above_y = boundary_y - dy;
                            floor_above_x = tx;
                        }
                        if (floor_below_y < 0 && in_bounds_fully(boundary_y + dy, tx) 
                            && cave_floor_bold(boundary_y + dy, tx))
                        {
                            floor_below_y = boundary_y + dy;
                            floor_below_x = tx;
                        }
                    }
                }
                
                /* If both partitions have floor nearby, check if connection needed */
                if (floor_above_y >= 0 && floor_below_y >= 0)
                {
                    /* Check if boundary is already connected */
                    bool boundary_connected = false;
                    for (int dx = -3; dx <= 3; ++dx)
                    {
                        int tx = cx + dx;
                        for (int dy = -2; dy <= 2; ++dy)
                        {
                            if (in_bounds_fully(boundary_y + dy, tx) && cave_floor_bold(boundary_y + dy, tx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }
                    
                    if (!boundary_connected)
                    {
                        /* Carve from floor_above to floor_below through the boundary */
                        int mid_x = (floor_above_x + floor_below_x) / 2;
                        
                        /* Carve vertical corridor centered on boundary */
                        for (int dy = -CORRIDOR_LEN; dy <= CORRIDOR_LEN; ++dy)
                        {
                            int ty = boundary_y + dy;
                            if (in_bounds_fully(ty, mid_x) && 
                                (cave_feat[ty][mid_x] == FEAT_WALL_EXTRA || cave_feat[ty][mid_x] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(ty, mid_x, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("H-boundary row=%d col=%d: carved at x=%d from y=%d to y=%d",
                                       row, col, mid_x, boundary_y - CORRIDOR_LEN, boundary_y + CORRIDOR_LEN);
                        break;  /* Only one connection per segment needed */
                    }
                }
            }
        }
    }
    
    /* Create vertical boundary connections (between columns) */
    for (int col = 0; col < grid_cols - 1; ++col)
    {
        int boundary_x = ((col + 1) * p_ptr->cur_map_wid / grid_cols);
        
        for (int row = 0; row < grid_rows; ++row)
        {
            int y1 = (row * p_ptr->cur_map_hgt / grid_rows) + 2;
            int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 2;
            
            for (int attempt = 0; attempt < ATTEMPTS_PER_SEGMENT; ++attempt)
            {
                int cy = rand_range(y1 + 2, y2 - 2);
                
                int floor_left_y = -1, floor_left_x = -1;
                int floor_right_y = -1, floor_right_x = -1;
                
                for (int dy = -5; dy <= 5; ++dy)
                {
                    int ty = cy + dy;
                    if (ty < 1 || ty >= p_ptr->cur_map_hgt - 1) continue;
                    
                    for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
                    {
                        if (floor_left_x < 0 && in_bounds_fully(ty, boundary_x - dx) 
                            && cave_floor_bold(ty, boundary_x - dx))
                        {
                            floor_left_y = ty;
                            floor_left_x = boundary_x - dx;
                        }
                        if (floor_right_x < 0 && in_bounds_fully(ty, boundary_x + dx) 
                            && cave_floor_bold(ty, boundary_x + dx))
                        {
                            floor_right_y = ty;
                            floor_right_x = boundary_x + dx;
                        }
                    }
                }
                
                if (floor_left_x >= 0 && floor_right_x >= 0)
                {
                    bool boundary_connected = false;
                    for (int dy = -3; dy <= 3; ++dy)
                    {
                        int ty = cy + dy;
                        for (int dx = -2; dx <= 2; ++dx)
                        {
                            if (in_bounds_fully(ty, boundary_x + dx) && cave_floor_bold(ty, boundary_x + dx))
                            {
                                boundary_connected = true;
                                break;
                            }
                        }
                        if (boundary_connected) break;
                    }
                    
                    if (!boundary_connected)
                    {
                        int mid_y = (floor_left_y + floor_right_y) / 2;
                        
                        for (int dx = -CORRIDOR_LEN; dx <= CORRIDOR_LEN; ++dx)
                        {
                            int tx = boundary_x + dx;
                            if (in_bounds_fully(mid_y, tx) && 
                                (cave_feat[mid_y][tx] == FEAT_WALL_EXTRA || cave_feat[mid_y][tx] == FEAT_WALL_OUTER))
                            {
                                cave_set_feat(mid_y, tx, FEAT_FLOOR);
                            }
                        }
                        connections_added++;
                        genlog_connect("V-boundary row=%d col=%d: carved at y=%d from x=%d to x=%d",
                                       row, col, mid_y, boundary_x - CORRIDOR_LEN, boundary_x + CORRIDOR_LEN);
                        break;
                    }
                }
            }
        }
    }
    
    if (connections_added > 0)
    {
        log_trace("Partition connectivity: added %d boundary connections", connections_added);
        genlog_connect("Partition connectivity: added %d boundary connections total", connections_added);
    }
    else
    {
        genlog_connect("Partition connectivity: no new connections needed");
    }
}
#endif

typedef struct {
    rectangle bounds;
    coord center;
    int rooms[CENT_MAX];
    int room_count;
    int hub_room;
} partition_link_data_t;

static int partition_index_from_point(int y, int x, int rows, int cols)
{
    if (rows <= 0 || cols <= 0) return -1;
    if (p_ptr->cur_map_hgt <= 0 || p_ptr->cur_map_wid <= 0) return -1;
    int row = (y * rows) / p_ptr->cur_map_hgt;
    int col = (x * cols) / p_ptr->cur_map_wid;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= rows) row = rows - 1;
    if (col >= cols) col = cols - 1;
    return row * cols + col;
}

static int room_connection_degree(int room_idx)
{
    if (room_idx < 0 || room_idx >= dun->cent_n)
        return 0;
    int deg = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (dun->connection[room_idx][i])
            deg++;
    }
    return deg;
}

static bool connect_rooms_with_logging(int r1, int r2, const char *tag, bool allow_desperate)
{
    if (r1 < 0 || r2 < 0 || r1 == r2)
        return false;

    if (dun->connection[r1][r2])
        return true;

    bool ok = connect_two_rooms(r1, r2, true, false);
    if (!ok && allow_desperate)
        ok = connect_two_rooms(r1, r2, true, true);

    if (ok && tag)
    {
        int dist = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
        genlog_connect("%s: linked room %d -> %d (dist=%d)", tag, r1, r2, dist);
    }
    return ok;
}

static bool is_big_partition_mode(quadrant_mode_t mode)
{
    return (mode == QUAD_MODE_LABYRINTH || mode == QUAD_MODE_BIG_CAVE || mode == QUAD_MODE_CHASM);
}

static bool big_partition_boundary_floor_ok(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (!cave_floor_bold(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_CHASM)
        return false;
    if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
        return false;
    return true;
}

/* Fallback connector for adjacent big partitions.
 * Standard tunnel rules often reject tunneling between open areas, so when
 * two big partitions border each other we carve a straight "doorway" across
 * their shared boundary, turning WALL_OUTER into doors and WALL_EXTRA into floor. */
static bool carve_straight_big_partition_connector(
    int y1, int x1, int y2, int x2, int r1, int r2)
{
    int dy = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;
    int dx = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;

    /* Must be a straight segment. */
    if (!((dy == 0) ^ (dx == 0)))
        return false;

    if (morgoth_segment_blocked(y1, x1, y2, x2, 2))
        return false;

    bool carved = false;
    int y = y1;
    int x = x1;

    for (;;)
    {
        if (!in_bounds_fully(y, x))
            return false;

        if (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
            return false;

        int feat = cave_feat[y][x];
        if (feat == FEAT_WALL_PERM)
            return false;

        if (feat == FEAT_WALL_OUTER)
        {
            cave_set_feat(y, x, FEAT_DOOR_HEAD);
            carved = true;
        }
        else if (feat == FEAT_WALL_EXTRA || feat == FEAT_CHASM)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            carved = true;
        }
        else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
        {
            /* Don't carve through inner/solid room walls, rubble walls, etc. */
            if (feat != FEAT_WALL_EXTRA)
                return false;
        }
        else if (feature_is_any_door(feat) || feat == FEAT_FLOOR)
        {
            /* Already passable; keep it. */
        }
        else
        {
            /* Avoid unexpected terrain (stairs, traps, etc.) */
            return false;
        }

        if (y == y2 && x == x2)
            break;
        y += dy;
        x += dx;
    }

    return carved;
}

static bool connect_adjacent_big_partitions_by_boundary(
    int pi_a, int pi_b, const rectangle *bounds_a, const rectangle *bounds_b,
    int rows, int cols, int hub_a, int hub_b, bool vertical_boundary)
{
    const int SEARCH_DEPTH = 24;

    if (hub_a < 0 || hub_b < 0)
        return false;

    if (!bounds_a || !bounds_b)
        return false;

    if (vertical_boundary)
    {
        /* A is left of B: boundary at the start column of B. */
        int boundary_x = ((pi_b % cols) * p_ptr->cur_map_wid / cols);
        int y_lo = MAX(bounds_a->y1, bounds_b->y1);
        int y_hi = MIN(bounds_a->y2, bounds_b->y2);

        boundary_x = MAX(1, MIN(p_ptr->cur_map_wid - 2, boundary_x));
        if (y_hi - y_lo < 6)
            return false;

        int best_y = -1;
        int best_left = -1;
        int best_right = -1;
        int best_len = 999999;

        for (int y = y_lo + 2; y <= y_hi - 2; ++y)
        {
            int x_left = -1;
            for (int dx = 1; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x - dx;
                if (x < bounds_a->x1)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_a)
                    continue;
                if (big_partition_boundary_floor_ok(y, x))
                {
                    x_left = x;
                    break;
                }
            }

            int x_right = -1;
            for (int dx = 0; dx <= SEARCH_DEPTH; ++dx)
            {
                int x = boundary_x + dx;
                if (x > bounds_b->x2)
                    break;
                if (partition_index_from_point(y, x, rows, cols) != pi_b)
                    continue;
                if (big_partition_boundary_floor_ok(y, x))
                {
                    x_right = x;
                    break;
                }
            }

            if (x_left >= 0 && x_right >= 0 && x_left < x_right)
            {
                int len = x_right - x_left;
                if (len < best_len || (len == best_len && one_in_(2)))
                {
                    best_len = len;
                    best_y = y;
                    best_left = x_left;
                    best_right = x_right;
                }
            }
        }

        if (best_y < 0)
            return false;

        if (!carve_straight_big_partition_connector(best_y, best_left, best_y, best_right, hub_a, hub_b))
            return false;

        dun->connection[hub_a][hub_b] = true;
        dun->connection[hub_b][hub_a] = true;
        genlog_connect("Big partition boundary: carved H link rooms %d<->%d at y=%d x=%d..%d",
            hub_a, hub_b, best_y, best_left, best_right);
        return true;
    }

    /* Horizontal boundary: A is above B. */
    int boundary_y = ((pi_b / cols) * p_ptr->cur_map_hgt / rows);
    int x_lo = MAX(bounds_a->x1, bounds_b->x1);
    int x_hi = MIN(bounds_a->x2, bounds_b->x2);

    boundary_y = MAX(1, MIN(p_ptr->cur_map_hgt - 2, boundary_y));
    if (x_hi - x_lo < 6)
        return false;

    int best_x = -1;
    int best_up = -1;
    int best_down = -1;
    int best_len = 999999;

    for (int x = x_lo + 2; x <= x_hi - 2; ++x)
    {
        int y_up = -1;
        for (int dy = 1; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y - dy;
            if (y < bounds_a->y1)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_a)
                continue;
            if (big_partition_boundary_floor_ok(y, x))
            {
                y_up = y;
                break;
            }
        }

        int y_down = -1;
        for (int dy = 0; dy <= SEARCH_DEPTH; ++dy)
        {
            int y = boundary_y + dy;
            if (y > bounds_b->y2)
                break;
            if (partition_index_from_point(y, x, rows, cols) != pi_b)
                continue;
            if (big_partition_boundary_floor_ok(y, x))
            {
                y_down = y;
                break;
            }
        }

        if (y_up >= 0 && y_down >= 0 && y_up < y_down)
        {
            int len = y_down - y_up;
            if (len < best_len || (len == best_len && one_in_(2)))
            {
                best_len = len;
                best_x = x;
                best_up = y_up;
                best_down = y_down;
            }
        }
    }

    if (best_x < 0)
        return false;

    if (!carve_straight_big_partition_connector(best_up, best_x, best_down, best_x, hub_a, hub_b))
        return false;

    dun->connection[hub_a][hub_b] = true;
    dun->connection[hub_b][hub_a] = true;
    genlog_connect("Big partition boundary: carved V link rooms %d<->%d at x=%d y=%d..%d",
        hub_a, hub_b, best_x, best_up, best_down);
    return true;
}

static void seed_partition_adjacency(const int *room_to_part, int part_count, bool adj[25][25], int degree[25])
{
    for (int i = 0; i < part_count; ++i)
        degree[i] = 0;

    for (int i = 0; i < part_count; ++i)
        for (int j = 0; j < part_count; ++j)
            adj[i][j] = false;

    for (int a = 0; a < dun->cent_n; ++a)
    {
        int pa = (a < CENT_MAX) ? room_to_part[a] : -1;
        if (pa < 0 || pa >= part_count) continue;

        for (int b = a + 1; b < dun->cent_n; ++b)
        {
            if (!dun->connection[a][b]) continue;
            int pb = (b < CENT_MAX) ? room_to_part[b] : -1;
            if (pb < 0 || pb >= part_count || pb == pa) continue;
            if (!adj[pa][pb])
            {
                adj[pa][pb] = adj[pb][pa] = true;
                degree[pa]++;
                degree[pb]++;
            }
        }
    }
}

static void mark_partition_edge(int p1, int p2, bool adj[25][25], int degree[25])
{
    if (p1 < 0 || p2 < 0 || p1 >= 25 || p2 >= 25 || p1 == p2)
        return;
    if (!adj[p1][p2])
    {
        adj[p1][p2] = adj[p2][p1] = true;
        degree[p1]++;
        degree[p2]++;
    }
}

static int choose_partition_hub(const partition_link_data_t *part)
{
    int best = -1;
    int best_rank = -1;
    int best_area = -1;
    int best_dist = 999999;

    int limit = MIN(part->room_count, CENT_MAX);
    for (int i = 0; i < limit; ++i)
    {
        int r = part->rooms[i];
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int dist = distance(dun->cent[r].y, dun->cent[r].x, part->center.y, part->center.x);
        int rank = room_anchor_requires_neighbor[r] ? 2 :
                   (room_anchor_kind[r] != LAYOUT_ANCHOR_NONE ? 1 : 0);

        if (rank > best_rank ||
            (rank == best_rank && area > best_area) ||
            (rank == best_rank && area == best_area && dist < best_dist))
        {
            best = r;
            best_rank = rank;
            best_area = area;
            best_dist = dist;
        }
    }
    return best;
}

static int find_anchor_target(int src, const int *room_to_part, const bool *skip, int part_count)
{
    int src_part = (src >= 0 && src < CENT_MAX) ? room_to_part[src] : -1;
    int src_piece = (src >= 0 && src < dun->cent_n) ? dun->piece[src] : -1;
    int best = -1;
    int best_tier = 10;
    int best_dist = 999999;

    for (int r = 0; r < dun->cent_n; ++r)
    {
        if (r == src) continue;
        if (skip && skip[r]) continue;
        if (dun->connection[src][r]) continue;

        int tier = 2;
        if (src_piece > 0 && dun->piece[r] > 0 && dun->piece[r] != src_piece)
            tier = 0;
        else if (room_to_part && r < CENT_MAX && room_to_part[r] != src_part)
            tier = 1;

        if (part_count > 0 && room_to_part && (room_to_part[r] < 0 || room_to_part[r] >= part_count))
            continue;

        int dist = distance(dun->cent[src].y, dun->cent[src].x, dun->cent[r].y, dun->cent[r].x);
        if (tier < best_tier || (tier == best_tier && dist < best_dist))
        {
            best_tier = tier;
            best_dist = dist;
            best = r;
        }
    }
    return best;
}

static void connect_anchor_backbone(const int *room_to_part, int part_count)
{
    if (layout_anchor_count <= 0 || dun->cent_n <= 0)
        return;

    (void)dungeon_pieces();

    int anchors_linked = 0;
    int anchors_considered = 0;

    for (int i = 0; i < layout_anchor_count; ++i)
    {
        int r = layout_anchors[i].room_slot;
        if (r < 0 || r >= dun->cent_n)
            continue;

        anchors_considered++;
        int area = (dun->corner[r].y2 - dun->corner[r].y1 + 1) *
                   (dun->corner[r].x2 - dun->corner[r].x1 + 1);
        int target_degree = 1;
        if (layout_anchors[i].requires_neighbor)
            target_degree = 2;
        if (area >= 600)
            target_degree = MAX(target_degree, 2);
        if (area >= 900)
            target_degree = MAX(target_degree, 3);

        int deg = room_connection_degree(r);
        bool tried[CENT_MAX];
        for (int t = 0; t < CENT_MAX; ++t) tried[t] = false;

        int attempts = 0;
        while (deg < target_degree && attempts < 8)
        {
            attempts++;
            int target = find_anchor_target(r, room_to_part, tried, part_count);
            if (target < 0)
                break;

            tried[target] = true;
            if (connect_rooms_with_logging(r, target, "Anchor backbone", true))
            {
                anchors_linked++;
                deg++;
                (void)dungeon_pieces();
            }
        }
    }

    if (anchors_linked > 0)
    {
        genlog_connect("Anchor backbone: linked %d/%d anchors to reduce isolation", anchors_linked, anchors_considered);
    }
}

/* Add connective tissue between partitions by linking a representative room in each partition,
 * then ensure special anchors have multiple exits to avoid dead ends. */
static void connect_partition_hubs(void)
{
    int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
    int rows = current_partition_rows;
    int cols = current_partition_cols;
    int count = current_partition_count;

    if (rows <= 0 || cols <= 0) {
        fallback_partition_grid_from_blocks(blocks, &rows, &cols);
        count = rows * cols;
    }
    if (count <= 1 || rows <= 0 || cols <= 0)
        return;

    partition_link_data_t parts[25];
    int room_to_part[CENT_MAX];
    for (int i = 0; i < CENT_MAX; ++i) room_to_part[i] = -1;

    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        parts[pi].room_count = 0;
        parts[pi].hub_room = -1;
        int y1, y2, x1, x2;
        if (compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
        {
            parts[pi].bounds.y1 = y1;
            parts[pi].bounds.y2 = y2;
            parts[pi].bounds.x1 = x1;
            parts[pi].bounds.x2 = x2;
            parts[pi].center.y = (y1 + y2) / 2;
            parts[pi].center.x = (x1 + x2) / 2;
        }
    }

    for (int r = 0; r < dun->cent_n && r < CENT_MAX; ++r)
    {
        int pi = partition_index_from_point(dun->cent[r].y, dun->cent[r].x, rows, cols);
        room_to_part[r] = pi;
        if (pi < 0 || pi >= count || pi >= 25)
            continue;
        int idx = parts[pi].room_count++;
        if (idx < CENT_MAX)
            parts[pi].rooms[idx] = r;
    }

    for (int pi = 0; pi < count && pi < 25; ++pi)
        parts[pi].hub_room = choose_partition_hub(&parts[pi]);

    bool adj[25][25];
    int degree[25];
    seed_partition_adjacency(room_to_part, count, adj, degree);

    /* Connect adjacent big partitions (labyrinth, big_cave, chasm) FIRST before regular backbone */
    /* This ensures big partitions get priority connections to each other */
    int big_links = 0;
    int big_adjacencies_found = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            if (idx >= count || idx >= 25)
                continue;
            
            quadrant_mode_t mode = current_partition_modes[idx];
            bool is_big = is_big_partition_mode(mode);
            if (!is_big)
                continue;
            
            int hub_here = parts[idx].hub_room;
            if (hub_here < 0)
                continue;
            
            /* Check right neighbor */
            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                if (idx_r < count && idx_r < 25)
                {
                    quadrant_mode_t mode_r = current_partition_modes[idx_r];
                    bool is_big_r = is_big_partition_mode(mode_r);
                    if (is_big_r && !adj[idx][idx_r])
                    {
                        big_adjacencies_found++;
                        int hub_right = parts[idx_r].hub_room;
                        bool ok = false;
                        if (hub_right >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_right, "Big partition bridge H", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_r, &parts[idx].bounds, &parts[idx_r].bounds,
                                    rows, cols, hub_here, hub_right, true);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_r, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (horizontal)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_r == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_r == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col + 1);
                        }
                    }
                }
            }
            
            /* Check down neighbor */
            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                if (idx_d < count && idx_d < 25)
                {
                    quadrant_mode_t mode_d = current_partition_modes[idx_d];
                    bool is_big_d = is_big_partition_mode(mode_d);
                    if (is_big_d && !adj[idx][idx_d])
                    {
                        big_adjacencies_found++;
                        int hub_down = parts[idx_d].hub_room;
                        bool ok = false;
                        if (hub_down >= 0)
                        {
                            ok = connect_rooms_with_logging(hub_here, hub_down, "Big partition bridge V", true);
                            if (!ok)
                            {
                                ok = connect_adjacent_big_partitions_by_boundary(
                                    idx, idx_d, &parts[idx].bounds, &parts[idx_d].bounds,
                                    rows, cols, hub_here, hub_down, false);
                            }
                        }

                        if (ok)
                        {
                            mark_partition_edge(idx, idx_d, adj, degree);
                            big_links++;
                            genlog_connect("Big partition bridge: connected %s at [%d,%d] to %s at [%d,%d] (vertical)",
                                         mode == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row, col,
                                         mode_d == QUAD_MODE_LABYRINTH ? "LABYRINTH" : (mode_d == QUAD_MODE_BIG_CAVE ? "BIG_CAVE" : "CHASM"),
                                         row + 1, col);
                        }
                    }
                }
            }
        }
    }
    
    if (big_links > 0)
    {
        log_trace("Big partition bridges: added %d connections between adjacent labyrinths/caves/chasms (found %d adjacencies)", 
                  big_links, big_adjacencies_found);
        genlog_connect("Big partition bridges: connected %d pairs of adjacent big partitions", big_links);
    }

    /* Now run regular partition backbone connections */
    int links = 0;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            int idx = row * cols + col;
            int hub_here = (idx < 25) ? parts[idx].hub_room : -1;
            if (hub_here < 0)
                continue;

            if (col + 1 < cols)
            {
                int idx_r = row * cols + (col + 1);
                int hub_right = parts[idx_r].hub_room;
                if (hub_right >= 0 && connect_rooms_with_logging(hub_here, hub_right, "Partition backbone H", true))
                {
                    mark_partition_edge(idx, idx_r, adj, degree);
                    links++;
                }
            }

            if (row + 1 < rows)
            {
                int idx_d = (row + 1) * cols + col;
                int hub_down = parts[idx_d].hub_room;
                if (hub_down >= 0 && connect_rooms_with_logging(hub_here, hub_down, "Partition backbone V", true))
                {
                    mark_partition_edge(idx, idx_d, adj, degree);
                    links++;
                }
            }

            if (col + 1 < cols && row + 1 < rows)
            {
                int idx_dr = (row + 1) * cols + (col + 1);
                int hub_diag = parts[idx_dr].hub_room;
                if (hub_diag >= 0 && connect_rooms_with_logging(hub_here, hub_diag, "Partition backbone D", true))
                {
                    mark_partition_edge(idx, idx_dr, adj, degree);
                    links++;
                }
            }
        }
    }

    int target_degree = (count >= 3) ? 2 : 1;
    for (int pi = 0; pi < count && pi < 25; ++pi)
    {
        if (parts[pi].hub_room < 0)
            continue;
        if (degree[pi] >= target_degree)
            continue;

        int attempts = 0;
        while (degree[pi] < target_degree && attempts < count)
        {
            attempts++;
            int best = -1;
            int best_dist = 999999;
            for (int pj = 0; pj < count && pj < 25; ++pj)
            {
                if (pj == pi) continue;
                if (parts[pj].hub_room < 0) continue;
                if (adj[pi][pj]) continue;
                int dist = distance(parts[pi].center.y, parts[pi].center.x, parts[pj].center.y, parts[pj].center.x);
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best = pj;
                }
            }

            if (best < 0)
                break;

            if (connect_rooms_with_logging(parts[pi].hub_room, parts[best].hub_room, "Partition backbone fill", true))
            {
                mark_partition_edge(pi, best, adj, degree);
                links++;
            }
            else
            {
                adj[pi][best] = adj[best][pi] = true;
            }
        }
    }

    if (links > 0)
        log_trace("Partition hub pass: added %d backbone links (grid %dx%d)", links, rows, cols);

    connect_anchor_backbone(room_to_part, count);
}

/* Anchor-aware connector: link nearby anchors to reduce isolation without over-saturating tunnels. */
/* Repair all outer walls after generation - critical for tunnel connectivity.
 * This fixes cases where overlapping room/cave generation overwrote WALL_OUTER
 * tiles back to WALL_EXTRA, breaking tunnel connection logic. */
static void repair_all_outer_walls(void)
{
    int repaired = 0;
    
    /* Scan entire map for wall tiles that border CAVE_ROOM floor */
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            /* Skip if already floor or already outer wall */
            if (cave_floor_bold(y, x))
                continue;
            if (cave_feat[y][x] == FEAT_WALL_OUTER)
                continue;
            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
                continue;
            
            /* Check if this wall borders any CAVE_ROOM floor */
            bool borders_room_floor = false;
            for (int dy = -1; dy <= 1 && !borders_room_floor; ++dy)
            {
                for (int dx = -1; dx <= 1 && !borders_room_floor; ++dx)
                {
                    if (dy == 0 && dx == 0)
                        continue;
                    int ny = y + dy, nx = x + dx;
                    if (in_bounds_fully(ny, nx) && cave_floor_bold(ny, nx)
                        && (cave_info[ny][nx] & CAVE_ROOM))
                    {
                        borders_room_floor = true;
                    }
                }
            }
            
            if (borders_room_floor)
            {
                cave_set_feat(y, x, FEAT_WALL_OUTER);
                repaired++;
            }
        }
    }
    
    if (repaired > 0)
    {
        log_trace("repair_all_outer_walls: converted %d WALL_EXTRA to WALL_OUTER", repaired);
    }
}

/* Fallback builder to guarantee the minimum room count before connectivity work */
static void ensure_minimum_rooms(void)
{
    if (dun->cent_n >= room_capacity_limit())
        return;
    if (dun->cent_n >= ROOM_MIN)
        return;

    int before = dun->cent_n;
    /* Try a mix of simple rooms near the centre to avoid hard failures */
    for (int attempt = 0; attempt < 50 && dun->cent_n < ROOM_MIN && dun->cent_n < room_capacity_limit(); ++attempt)
    {
        int y = rand_range(4, p_ptr->cur_map_hgt - 4);
        int x = rand_range(4, p_ptr->cur_map_wid - 4);

        /* Alternate basic shapes to improve odds in cramped layouts */
        if (attempt % 3 == 0)
            build_type1(y, x);
        else if (attempt % 3 == 1)
            build_type2(y, x);
        else
            build_type6(y, x, false);
    }

    if (dun->cent_n > before)
    {
        log_trace("Room fallback: added %d emergency rooms (now %d)", dun->cent_n - before, dun->cent_n);
    }
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
    if (!in_bounds_fully(y, x)) return false;

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
            || ((feature >= FEAT_DOOR_HEAD) && (feature <= FEAT_DOOR_TAIL))
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
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int ddx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int head = 0;
    int tail = 0;

    if (!in_bounds_fully(y, x)) return;
    if (access_array[y][x]) return;
    if (!player_passable(y, x, ignore_rubble_and_chasms)) return;

    access_array[y][x] = true;
    queue[tail++] = y * MAX_DUNGEON_WID + x;

    while (head < tail)
    {
        int idx = queue[head++];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;

        for (int d = 0; d < 8; ++d)
        {
            int ny = cy + ddy[d];
            int nx = cx + ddx[d];

            if (!in_bounds_fully(ny, nx)) continue;
            if (access_array[ny][nx]) continue;
            if (!player_passable(ny, nx, ignore_rubble_and_chasms)) continue;

            access_array[ny][nx] = true;
            if (tail < (int)N_ELEMENTS(queue))
            {
                queue[tail++] = ny * MAX_DUNGEON_WID + nx;
            }
        }
    }
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
            int chance = 20 >> stacks;         /* base 1-in-20 Ã”Ã¥Ã† 1-in-10 Ã”Ã¥Ã† 1-in-5 */
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

/* Mode-specific drop tuning for partition floor/corridor scatter */
typedef struct partition_drop_profile {
    bool allow_floor_drops;
    int reroll_chance; /* percentage chance to re-roll placement in this partition */
    drop_profile profile;
} partition_drop_profile;

static quadrant_mode_t partition_mode_for_point(int y, int x)
{
    /* If we're on a loaded level (no generation metadata), infer a reasonable grid and
     * classify big partitions so runtime systems (drops, UI messages) can work. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
    {
        /* Candidate grids matching apply_quadrant_generation_modes() (including its random-orientation cases). */
        int blocks = (PANEL_HGT > 0) ? (p_ptr->cur_map_hgt / PANEL_HGT) : 0;
        int grids[2][2] = {{0, 0}, {0, 0}};
        int grid_count = 0;

        if (blocks > 0)
        {
            if (blocks <= 9)
            {
                grids[0][0] = 2; grids[0][1] = 2; grid_count = 1;
            }
            else if (blocks == 10)
            {
                grids[0][0] = 3; grids[0][1] = 2;
                grids[1][0] = 2; grids[1][1] = 3;
                grid_count = 2;
            }
            else if (blocks <= 13)
            {
                grids[0][0] = 3; grids[0][1] = 3; grid_count = 1;
            }
            else if (blocks == 14)
            {
                grids[0][0] = 3; grids[0][1] = 4;
                grids[1][0] = 4; grids[1][1] = 3;
                grid_count = 2;
            }
            else if (blocks <= 16)
            {
                grids[0][0] = 4; grids[0][1] = 4; grid_count = 1;
            }
            else if (blocks <= 20)
            {
                grids[0][0] = 5; grids[0][1] = 4;
                grids[1][0] = 4; grids[1][1] = 5;
                grid_count = 2;
            }
            else
            {
                grids[0][0] = 5; grids[0][1] = 5; grid_count = 1;
            }
        }

        int best_rows = 0, best_cols = 0;
        int best_score = -1000000;
        quadrant_mode_t best_modes[25];
        memset(best_modes, 0, sizeof(best_modes));

        for (int gi = 0; gi < grid_count; ++gi)
        {
            int rows = grids[gi][0];
            int cols = grids[gi][1];
            if (rows <= 0 || cols <= 0)
                continue;

            int count = rows * cols;
            if (count <= 0 || count > 25)
                continue;

            quadrant_mode_t modes_tmp[25];
            for (int i = 0; i < 25; ++i)
                modes_tmp[i] = QUAD_MODE_ROOMY;

            int chasm_parts = 0, labyrinth_parts = 0, cave_parts = 0;
            int total_chasm_tiles = 0, max_chasm_tiles = 0;

            for (int pi = 0; pi < count; ++pi)
            {
                int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
                if (!compute_partition_bounds(pi, rows, cols, &y1, &y2, &x1, &x2))
                    continue;

                int tiles = 0;
                int open_tiles = 0;
                int open_dead_ends = 0;
                int open_wide = 0;
                int open_corridor = 0;
                int chasm_tiles = 0;

                for (int yy = y1; yy <= y2; ++yy)
                {
                    for (int xx = x1; xx <= x2; ++xx)
                    {
                        if (!in_bounds_fully(yy, xx))
                            continue;
                        tiles++;

                        if ((cave_info[yy][xx] & CAVE_CHASM_AREA) || (cave_feat[yy][xx] == FEAT_CHASM))
                        {
                            chasm_tiles++;
                            continue;
                        }

                        if (!cave_floorlike_bold(yy, xx))
                            continue;

                        open_tiles++;

                        int n = 0;
                        if (in_bounds_fully(yy - 1, xx) && cave_floorlike_bold(yy - 1, xx) && cave_feat[yy - 1][xx] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy + 1, xx) && cave_floorlike_bold(yy + 1, xx) && cave_feat[yy + 1][xx] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy, xx - 1) && cave_floorlike_bold(yy, xx - 1) && cave_feat[yy][xx - 1] != FEAT_CHASM) n++;
                        if (in_bounds_fully(yy, xx + 1) && cave_floorlike_bold(yy, xx + 1) && cave_feat[yy][xx + 1] != FEAT_CHASM) n++;

                        if (n <= 1) open_dead_ends++;
                        if (n >= 3) open_wide++;
                        if (n == 2) open_corridor++;
                    }
                }

                total_chasm_tiles += chasm_tiles;
                if (chasm_tiles > max_chasm_tiles) max_chasm_tiles = chasm_tiles;

                quadrant_mode_t picked = QUAD_MODE_ROOMY;

                if (chasm_tiles > 0)
                {
                    picked = QUAD_MODE_CHASM;
                    chasm_parts++;
                }
                else if (tiles > 0 && open_tiles > 0)
                {
                    int open_pct = (open_tiles * 100) / tiles;
                    int wide_pct = (open_wide * 100) / open_tiles;
                    int dead_pct = (open_dead_ends * 100) / open_tiles;
                    int corridor_pct = (open_corridor * 100) / open_tiles;

                    /* BIG_CAVE: lots of open area, many wide tiles (3-4 neighbors). */
                    if (open_pct >= 38 && wide_pct >= 40)
                    {
                        picked = QUAD_MODE_BIG_CAVE;
                        cave_parts++;
                    }
                    /* LABYRINTH: corridor-dominated maze with relatively few open 'wide' tiles. */
                    else if (wide_pct <= 28 && corridor_pct >= 50 && dead_pct >= 8 && open_pct <= 55)
                    {
                        picked = QUAD_MODE_LABYRINTH;
                        labyrinth_parts++;
                    }
                }

                modes_tmp[pi] = picked;
            }

            /* Score grids that keep special features concentrated (avoid splitting a big area across partitions). */
            int score = 0;
            score -= (chasm_parts * 100);
            score -= ((labyrinth_parts + cave_parts) * 20);
            if (total_chasm_tiles > 0)
                score += (max_chasm_tiles * 500) / total_chasm_tiles;

            if (score > best_score)
            {
                best_score = score;
                best_rows = rows;
                best_cols = cols;
                memcpy(best_modes, modes_tmp, sizeof(best_modes));
            }
        }

        if (best_rows > 0 && best_cols > 0)
        {
            int count = best_rows * best_cols;
            remember_partition_grid(best_rows, best_cols, count);
            for (int i = 0; i < count; ++i)
                current_partition_modes[i] = best_modes[i];
            for (int i = count; i < 25; ++i)
                current_partition_modes[i] = QUAD_MODE_ROOMY;

            /* Densities are only used for generation decisions, so default to NORMAL. */
            for (int i = 0; i < 25; ++i)
                current_partition_densities[i] = DENSITY_NORMAL;

            log_trace("Inferred partition grid for runtime: blocks=%d grid=%dx%d score=%d",
                      blocks, best_rows, best_cols, best_score);
        }
    }

    int pi = partition_index_from_point(
        y, x, current_partition_rows, current_partition_cols);
    if (pi >= 0 && pi < current_partition_count)
        return current_partition_modes[pi];
    return QUAD_MODE_ROOMY;
}

/* Helper to find which room (if any) contains a given point */
static int room_index_for_point(int y, int x)
{
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (y >= dun->corner[i].y1 && y <= dun->corner[i].y2 &&
            x >= dun->corner[i].x1 && x <= dun->corner[i].x2)
        {
            return i;
        }
    }
    return -1;
}

/* Determine appropriate drop mode for a location based on room type */
static quadrant_mode_t drop_mode_for_point(int y, int x)
{
    /* Check if point is within a room */
    int room_idx = room_index_for_point(y, x);
    if (room_idx >= 0)
    {
        /* CA_BLOB rooms use CAVEY drops */
        if (room_anchor_kind[room_idx] == LAYOUT_ANCHOR_CA_BLOB)
            return QUAD_MODE_CAVEY;
        
        /* Regular rooms use ROOMY drops */
        if (cave_info[y][x] & CAVE_ROOM)
            return QUAD_MODE_ROOMY;
    }
    
    /* Fall back to partition mode for corridors and other areas */
    return partition_mode_for_point(y, x);
}

static level_partition_kind partition_kind_from_mode(quadrant_mode_t mode)
{
    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        return LEVEL_PART_ROOMY;
    case QUAD_MODE_CAVEY:
        return LEVEL_PART_CAVEY;
    case QUAD_MODE_RUINED:
        return LEVEL_PART_RUINED;
    case QUAD_MODE_LABYRINTH:
        return LEVEL_PART_LABYRINTH;
    case QUAD_MODE_CHASM:
        return LEVEL_PART_CHASM;
    case QUAD_MODE_BIG_CAVE:
        return LEVEL_PART_BIG_CAVE;
    default:
        return LEVEL_PART_NONE;
    }
}

static bool suppress_partition_effects_for_point(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    return (cave_info[y][x] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0;
}

level_partition_kind level_partition_kind_for_point(int y, int x)
{
    /* Suppress partition effects (labyrinth memory loss, big-cave penalties, etc.)
     * inside greater vault regions and Morgoth's entry tunnels. */
    if (suppress_partition_effects_for_point(y, x))
        return LEVEL_PART_ROOMY;

    /* Chests should follow the partition they spawned in (not room overrides). */
    quadrant_mode_t mode = partition_mode_for_point(y, x);
    return partition_kind_from_mode(mode);
}

void level_partition_meta_get(partition_meta_save* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    /* Populate metadata if this is a loaded level. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        (void)partition_mode_for_point(p_ptr->py, p_ptr->px);

    out->grid_rows = (s16b)current_partition_rows;
    out->grid_cols = (s16b)current_partition_cols;
    out->partition_count = (s16b)current_partition_count;

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->modes[i] = (byte)current_partition_modes[i];

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->big_cave_types[i] = (byte)current_partition_big_cave_types[i];
}

void level_partition_meta_set(const partition_meta_save* in)
{
    if (!in)
        return;

    int rows = in->grid_rows;
    int cols = in->grid_cols;
    int count = in->partition_count;

    if (rows <= 0 || cols <= 0 || count <= 0 || count > PARTITION_META_MAX || rows * cols != count)
    {
        current_partition_rows = 0;
        current_partition_cols = 0;
        current_partition_count = 0;
        for (int i = 0; i < PARTITION_META_MAX; ++i)
        {
            current_partition_modes[i] = QUAD_MODE_ROOMY;
            current_partition_densities[i] = DENSITY_NORMAL;
            current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        }
        return;
    }

    remember_partition_grid(rows, cols, count);
    for (int i = 0; i < PARTITION_META_MAX; ++i)
    {
        quadrant_mode_t mode = QUAD_MODE_ROOMY;
        big_cave_type_t cave_type = BIG_CAVE_NONE;
        if (i < count)
        {
            byte raw = in->modes[i];
            if (raw <= QUAD_MODE_BIG_CAVE)
                mode = (quadrant_mode_t)raw;
            if (mode == QUAD_MODE_BIG_CAVE)
            {
                byte raw_type = in->big_cave_types[i];
                if (raw_type > BIG_CAVE_NONE && raw_type < BIG_CAVE_TYPE_MAX)
                    cave_type = (big_cave_type_t)raw_type;
            }
        }
        current_partition_modes[i] = mode;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = cave_type;
    }
}

int level_partition_index_for_point(int y, int x)
{
    /* Ensure partition metadata exists even for loaded levels. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        (void)partition_mode_for_point(y, x);

    if (current_partition_rows <= 0 || current_partition_cols <= 0 || current_partition_count <= 0)
        return -1;

    int pi = partition_index_from_point(y, x, current_partition_rows, current_partition_cols);
    if (pi < 0 || pi >= current_partition_count)
        return -1;

    return pi;
}

big_cave_type_t level_partition_big_cave_type_for_index(int pi)
{
    if (pi < 0 || pi >= current_partition_count)
        return BIG_CAVE_NONE;
    if (current_partition_modes[pi] != QUAD_MODE_BIG_CAVE)
        return BIG_CAVE_NONE;
    return current_partition_big_cave_types[pi];
}

big_cave_type_t level_partition_big_cave_type_for_point(int y, int x)
{
    if (suppress_partition_effects_for_point(y, x))
        return BIG_CAVE_NONE;

    int pi = level_partition_index_for_point(y, x);
    if (pi < 0)
        return BIG_CAVE_NONE;
    return level_partition_big_cave_type_for_index(pi);
}

void level_layout_info_current(level_layout_info* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->map_wid = p_ptr->cur_map_wid;
    out->map_hgt = p_ptr->cur_map_hgt;
    out->partition_rows = current_partition_rows;
    out->partition_cols = current_partition_cols;
    out->partition_count = current_partition_count;

    int area_by_kind[LEVEL_PART_MAX] = {0};

    for (int i = 0; i < current_partition_count; ++i)
    {
        level_partition_kind kind = partition_kind_from_mode(current_partition_modes[i]);
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        int area = 0;

        if (compute_partition_bounds(
                i, current_partition_rows, current_partition_cols, &y1, &y2, &x1, &x2))
        {
            area = (y2 - y1 + 1) * (x2 - x1 + 1);
        }

        if (kind == LEVEL_PART_LABYRINTH)
            out->labyrinth_parts++;
        else if (kind == LEVEL_PART_BIG_CAVE)
            out->big_cave_parts++;
        else if (kind == LEVEL_PART_CHASM)
            out->chasm_parts++;

        if (kind > LEVEL_PART_NONE && kind < LEVEL_PART_MAX)
            area_by_kind[kind] += area;
    }

    const level_partition_kind preference[] = {LEVEL_PART_LABYRINTH,
        LEVEL_PART_BIG_CAVE, LEVEL_PART_CHASM, LEVEL_PART_RUINED,
        LEVEL_PART_CAVEY, LEVEL_PART_ROOMY};

    int dominant_area = 0;
    level_partition_kind dominant_kind = LEVEL_PART_NONE;
    for (size_t i = 0; i < N_ELEMENTS(preference); ++i)
    {
        level_partition_kind kind = preference[i];
        int area = area_by_kind[kind];
        if (area > dominant_area)
        {
            dominant_area = area;
            dominant_kind = kind;
        }
    }

    out->dominant_kind = dominant_kind;
}

static partition_drop_profile partition_drop_profile_for_mode(quadrant_mode_t mode)
{
    partition_drop_profile prof;
    prof.allow_floor_drops = true;
    prof.reroll_chance = 0;
    drop_profile_default(&prof.profile);

    switch (mode)
    {
    case QUAD_MODE_ROOMY:
        /* Default (ROOMY) a€” 40:30:10:20 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 10;
        prof.profile.weight_supply = 20;
        prof.profile.supply_potion = 1;
        prof.profile.supply_herb = 1;
        prof.profile.supply_gem = 1;
        prof.profile.supply_staff = 1;
        prof.profile.supply_misc = 1;
        break;
    case QUAD_MODE_LABYRINTH:
        /* LABYRINTH - 0:0:35:65 */
        prof.profile.weight_weapon = 0;
        prof.profile.weight_armor = 0;
        prof.profile.weight_jewelry = 35;
        prof.profile.weight_supply = 65;
        prof.profile.supply_potion = 15;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 15;
        prof.profile.supply_misc = 5;
        break;
    case QUAD_MODE_RUINED:
        /* RUINED 40:35:0:25 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 35;
        prof.profile.weight_jewelry = 0;
        prof.profile.weight_supply = 25;
        prof.profile.supply_potion = 7;
        prof.profile.supply_herb = 2;
        prof.profile.supply_gem = 1;
        prof.profile.supply_staff = 3;
        prof.profile.supply_misc = 20; /* torches, horns, arrows */
        prof.profile.supply_tunneling = 2; /* small chance for shovels/mattocks */
        break;
    case QUAD_MODE_CAVEY:
        prof.allow_floor_drops = false;
        break;
    case QUAD_MODE_BIG_CAVE:
        /* BIG_CAVE 20:20:15:45 (half usual drops) */
        prof.reroll_chance = 50;
        prof.profile.weight_weapon = 20;
        prof.profile.weight_armor = 20;
        prof.profile.weight_jewelry = 15;
        prof.profile.weight_supply = 45;
        prof.profile.supply_potion = 1;
        prof.profile.supply_herb = 3;
        prof.profile.supply_gem = 2;
        prof.profile.supply_staff = 1;
        prof.profile.supply_misc = 1;
        break;
    case QUAD_MODE_CHASM:
        /* CHASM 40:30:20:10 */
        prof.profile.weight_weapon = 40;
        prof.profile.weight_armor = 30;
        prof.profile.weight_jewelry = 20;
        prof.profile.weight_supply = 10;
        prof.profile.supply_potion = 1;
        prof.profile.supply_herb = 1;
        prof.profile.supply_gem = 1;
        prof.profile.supply_staff = 1;
        prof.profile.supply_misc = 1;
        break;
    default:
        break;
    }

    return prof;
}

static drop_profile drop_profile_for_mode(quadrant_mode_t mode)
{
    partition_drop_profile prof = partition_drop_profile_for_mode(mode);
    return prof.profile;
}

void drop_profile_for_partition_kind(level_partition_kind kind, drop_profile* out)
{
    if (!out)
        return;

    quadrant_mode_t mode = QUAD_MODE_ROOMY;
    switch (kind)
    {
    case LEVEL_PART_ROOMY:
        mode = QUAD_MODE_ROOMY;
        break;
    case LEVEL_PART_CAVEY:
        mode = QUAD_MODE_CAVEY;
        break;
    case LEVEL_PART_RUINED:
        mode = QUAD_MODE_RUINED;
        break;
    case LEVEL_PART_LABYRINTH:
        mode = QUAD_MODE_LABYRINTH;
        break;
    case LEVEL_PART_CHASM:
        mode = QUAD_MODE_CHASM;
        break;
    case LEVEL_PART_BIG_CAVE:
        mode = QUAD_MODE_BIG_CAVE;
        break;
    case LEVEL_PART_NONE:
    case LEVEL_PART_MAX:
    default:
        mode = QUAD_MODE_ROOMY;
        break;
    }

    *out = drop_profile_for_mode(mode);
}

static void place_object_with_profile_params(
    int y, int x, int base_depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, bool allow_artefacts,
    int artefact_weight_multiplier, u32b extra_ident,
    const partition_drop_profile* prof);

static void place_object_with_profile(
    int y, int x, const partition_drop_profile* prof)
{
    place_object_with_profile_params(
        y, x, object_level, object_level, DROP_QUALITY_NORMAL, DROP_TYPE_UNTHEMED,
        false, 1, 0, prof);
}

static void place_object_with_profile_params(
    int y, int x, int base_depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, bool allow_artefacts,
    int artefact_weight_multiplier, u32b extra_ident,
    const partition_drop_profile* prof)
{
    if (!in_bounds(y, x))
        return;
    if (!cave_clean_bold(y, x))
        return;

    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    object_wipe(i_ptr);

    int attempts = 0;
    const drop_profile* dp = (prof) ? &prof->profile : NULL;

    while (!drop_generate_object_profiled_depths_biased(base_depth,
               min_depth_penalty_depth, quality, droptype, 0, allow_artefacts,
               artefact_weight_multiplier, dp, i_ptr))
    {
        attempts++;
        if (attempts > 200)
            return;
    }

    if (i_ptr->tval == TV_CHEST)
        i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));
    if (extra_ident)
        i_ptr->ident |= extra_ident;

    if (!floor_carry(y, x, i_ptr))
    {
        a_info[i_ptr->name1].cur_num = 0;
    }
}

static int place_ruined_partition_damaged_items(
    int y1, int y2, int x1, int x2, int target_count)
{
    int placed = 0;
    partition_drop_profile prof = partition_drop_profile_for_mode(QUAD_MODE_RUINED);

    for (int n = 0; n < target_count; ++n)
    {
        bool placed_this = false;

        for (int tries = 0; tries < 200; ++tries)
        {
            int y = rand_range(y1, y2);
            int x = rand_range(x1, x2);

            if (!in_bounds_fully(y, x))
                continue;
            if (cave_info[y][x] & CAVE_G_VAULT)
                continue;
            if (partition_mode_for_point(y, x) != QUAD_MODE_RUINED)
                continue;
            if (cave_feat[y][x] == FEAT_RUBBLE)
                continue;
            if (!cave_clean_bold(y, x))
                continue;

            place_object_with_profile_params(
                y, x, object_level, object_level, DROP_QUALITY_NORMAL,
                DROP_TYPE_DAMAGED, false, 1, 0, &prof);

            if (cave_o_idx[y][x] != 0)
            {
                placed++;
                placed_this = true;
                break;
            }
        }

        if (!placed_this)
            break;
    }

    return placed;
}

/*
 * Allocates some objects (using "place" and "type") in a specific partition
 */
static void alloc_object_in_partition(int set, int typ, int num, quadrant_mode_t target_mode)
{
    int y, x, k, i;

    /* Place some objects */
    for (k = 0; k < num; k++)
    {
        partition_drop_profile active_profile =
            partition_drop_profile_for_mode(target_mode);
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

            /* Check if location is in target partition */
            if (partition_mode_for_point(y, x) != target_mode)
                continue;

            /* Check for "room" */
            is_room = (cave_info[y][x] & (CAVE_ROOM)) ? true : false;

            /* Require corridor? */
            if ((set == ALLOC_SET_CORR) && is_room)
                continue;

            /* Require room? */
            if ((set == ALLOC_SET_ROOM) && !is_room)
                continue;

            /* Enforce room-type and partition-specific drop behaviour */
            quadrant_mode_t mode = drop_mode_for_point(y, x);
            active_profile = partition_drop_profile_for_mode(mode);
            if (typ == ALLOC_TYP_OBJECT)
            {
                if (!active_profile.allow_floor_drops)
                    continue;

                if (active_profile.reroll_chance > 0
                    && rand_int(100) < active_profile.reroll_chance)
                    continue;
            }

            /* Accept it */
            break;
        }

        /* Failed to place object */
        if (i >= 10000)
            continue;

        /* Place something */
        switch (typ)
        {
        case ALLOC_TYP_RUBBLE:
        {
            /* Create rubble */
            cave_set_feat(y, x, FEAT_RUBBLE);

            break;
        }

        case ALLOC_TYP_OBJECT:
        {
            /* Ruined damaged gear is seeded separately during partition generation. */
            place_object_with_profile(y, x, &active_profile);
            break;
        }
        }
    }
}

/*
 * Allocates some objects (using "place" and "type") globally (not partition-specific)
 * Used for rubble and other non-object placement
 */
static void alloc_object_global(int set, int typ, int num, bool out_of_sight)
{
    int y, x, k, i;

    /* Place some objects */
    for (k = 0; k < num; k++)
    {
        partition_drop_profile active_profile =
            partition_drop_profile_for_mode(QUAD_MODE_ROOMY);
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

            /* Enforce room-type and partition-specific drop behaviour */
            quadrant_mode_t mode = drop_mode_for_point(y, x);
            active_profile = partition_drop_profile_for_mode(mode);
            if (typ == ALLOC_TYP_OBJECT)
            {
                if (!active_profile.allow_floor_drops)
                    continue;

                if (active_profile.reroll_chance > 0
                    && rand_int(100) < active_profile.reroll_chance)
                    continue;
            }

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
            place_object_with_profile(y, x, &active_profile);
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
    if ((p_ptr->depth > 2) && (p_ptr->depth < MORGOTH_DEPTH)
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
    /* Don't dig L-corridors when the corner is too close to non-room empty space.
     * But allow corners near CAVE_ROOM floor (from caves, chasms, etc.) */
    if (!(cave_info[y][x_lo] & (CAVE_ROOM)))
    {
        bool blocked_lo = false;
        if (cave_feat[y - 1][x_lo - 1] == FEAT_FLOOR && !(cave_info[y - 1][x_lo - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (cave_feat[y + 1][x_lo - 1] == FEAT_FLOOR && !(cave_info[y + 1][x_lo - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (blocked_lo)
            return (false);
    }
    if (!(cave_info[y][x_hi] & (CAVE_ROOM)))
    {
        bool blocked_hi = false;
        if (cave_feat[y - 1][x_hi + 1] == FEAT_FLOOR && !(cave_info[y - 1][x_hi + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (cave_feat[y + 1][x_hi + 1] == FEAT_FLOOR && !(cave_info[y + 1][x_hi + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (blocked_hi)
            return (false);
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
         * a room, UNLESS that adjacent floor is part of a CAVE_ROOM (cave edges) */
        if (!(cave_info[y][x] & (CAVE_ROOM)))
        {
            bool has_non_room_floor_adj = false;
            if (cave_feat[y + 1][x] == FEAT_FLOOR && !(cave_info[y + 1][x] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (cave_feat[y - 1][x] == FEAT_FLOOR && !(cave_info[y - 1][x] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (has_non_room_floor_adj)
            {
                return (false);
            }
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
    /* Don't dig L-corridors when the corner is too close to non-room empty space.
     * But allow corners near CAVE_ROOM floor (from caves, chasms, etc.) */
    if (!(cave_info[y_lo][x] & (CAVE_ROOM)))
    {
        bool blocked_lo = false;
        if (cave_feat[y_lo - 1][x - 1] == FEAT_FLOOR && !(cave_info[y_lo - 1][x - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (cave_feat[y_lo - 1][x + 1] == FEAT_FLOOR && !(cave_info[y_lo - 1][x + 1] & CAVE_ROOM))
            blocked_lo = true;
        if (blocked_lo)
            return (false);
    }
    if (!(cave_info[y_hi][x] & (CAVE_ROOM)))
    {
        bool blocked_hi = false;
        if (cave_feat[y_hi + 1][x - 1] == FEAT_FLOOR && !(cave_info[y_hi + 1][x - 1] & CAVE_ROOM))
            blocked_hi = true;
        if (cave_feat[y_hi + 1][x + 1] == FEAT_FLOOR && !(cave_info[y_hi + 1][x + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (blocked_hi)
            return (false);
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
         * a room, UNLESS that adjacent floor is part of a CAVE_ROOM (cave edges) */
        if (!(cave_info[y][x] & (CAVE_ROOM)))
        {
            bool has_non_room_floor_adj = false;
            if (cave_feat[y][x + 1] == FEAT_FLOOR && !(cave_info[y][x + 1] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (cave_feat[y][x - 1] == FEAT_FLOOR && !(cave_info[y][x - 1] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (has_non_room_floor_adj)
            {
                return (false);
            }
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

    /* On shallow branches, fall back to narrow connectors */
    if (tentative)
    {
        /* allow style variation even on tentative digs */
    }

    int depth = p_ptr->depth;
    int sidx = styles_get_level_primary_style();
    byte style_group = (sidx >= 0 && style_info) ? style_info[sidx].group : 0;
    bool style_grand = (style_group >= 4); /* warmer/darker palettes get a bump */

    /* Variable tunnel widths at any depth, probability scales with depth */
    /* Base rarity values (lower = more common) */
    int medium_rarity, grand_rarity;
    
    if (depth >= 20)
    {
        medium_rarity = style_grand ? 5 : 7;
        grand_rarity = style_grand ? 8 : 12;
    }
    else if (depth >= 12)
    {
        medium_rarity = style_grand ? 7 : 10;
        grand_rarity = style_grand ? 11 : 16;
    }
    else if (depth >= 7)
    {
        medium_rarity = style_grand ? 10 : 14;
        grand_rarity = style_grand ? 16 : 22;
    }
    else
    {
        /* Even early levels can have occasional wider corridors */
        medium_rarity = style_grand ? 16 : 20;
        grand_rarity = style_grand ? 25 : 30;
    }

    if (one_in_(grand_rarity))
    {
        profile.width = 3;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_PILLARS : TUNNEL_TREAT_NICHES;
    }
    else if (one_in_(medium_rarity))
    {
        profile.width = one_in_(4) ? 3 : 2;
        profile.side_bias = one_in_(2) ? 1 : -1;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_NICHES : TUNNEL_TREAT_NONE;
    }

    return profile;
}

static void apply_tunnel_niche_torch_glow(int niche_y, int niche_x, int front_dy, int front_dx)
{
    if (!in_bounds_fully(niche_y, niche_x))
        return;

    /* "Torch" effect (radius 1) biased into the corridor:
     * - light the niche floor itself
     * - light the two wall tiles flanking the niche (along the corridor axis)
     * - light the 3 corridor floor tiles directly in front of the niche
     */
    int axis_dy = (front_dx != 0) ? 1 : 0;
    int axis_dx = (front_dy != 0) ? 1 : 0;

    if (cave_floor_bold(niche_y, niche_x)
        && !(cave_info[niche_y][niche_x] & (CAVE_ROOM | CAVE_ICKY)))
    {
        cave_info[niche_y][niche_x] |= (CAVE_GLOW);
    }

    for (int i = -RADIUS_TORCH; i <= RADIUS_TORCH; i += 2 * RADIUS_TORCH)
    {
        int wy = niche_y + axis_dy * i;
        int wx = niche_x + axis_dx * i;
        if (!in_bounds_fully(wy, wx))
            continue;
        if (cave_info[wy][wx] & (CAVE_ROOM | CAVE_ICKY))
            continue;
        if (cave_wall_bold(wy, wx))
            cave_info[wy][wx] |= (CAVE_GLOW);
    }

    int entry_y = niche_y + front_dy;
    int entry_x = niche_x + front_dx;
    for (int i = -RADIUS_TORCH; i <= RADIUS_TORCH; ++i)
    {
        int fy = entry_y + axis_dy * i;
        int fx = entry_x + axis_dx * i;
        if (!in_bounds_fully(fy, fx))
            continue;
        if (!cave_floor_bold(fy, fx))
            continue;
        if (cave_info[fy][fx] & (CAVE_ROOM | CAVE_ICKY))
            continue;
        cave_info[fy][fx] |= (CAVE_GLOW);
    }
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

                int dir = (side > 0) ? 1 : -1;
                apply_tunnel_niche_torch_glow(y, nx, 0, -dir);
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

                int dir = (side > 0) ? 1 : -1;
                apply_tunnel_niche_torch_glow(ny, x, -dir, 0);
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
    int morgoth_margin = 1;

    /* Allow long corridor spans across 3x3 partitions on 15x15 block maps */
    int base_limit_x = MAX(50, (p_ptr->cur_map_wid * 2) / 3); /* ~110 on 165x165 */
    int base_limit_y = MAX(35, (p_ptr->cur_map_hgt * 2) / 3); /* ~110 on 165x165 */
    int distance_limitx = desperate ? base_limit_x + base_limit_x / 2 : base_limit_x;
    int distance_limity = desperate ? base_limit_y + base_limit_y / 2 : base_limit_y;

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

    if (morgoth_region_active())
    {
        /* Skip any corridor that would cross the throne room partition */
        if (morgoth_segment_blocked(r1y, r1x, r2y, r2x, morgoth_margin))
            return false;
    }

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

        if (morgoth_segment_blocked(r1y, x, r2y, x, morgoth_margin))
            return false;
        success = build_tunnel(r1, r2, r1y, x, r2y, x, tentative);
    }
    /* if horizontal overlap */
    else if ((r1y1 <= r2y2) && (r2y1 <= r1y2))
    {
        y = rand_range(MAX(r1y1, r2y1),
            MIN(r1y2,
                r2y2)); // Sil-x: one of these two lines has somehow caused a
                        // crash

        if (morgoth_segment_blocked(y, r1x, y, r2x, morgoth_margin))
            return false;
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

        if (morgoth_segment_blocked(r1y, r1x, r1y, r2x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r1y, r2x, r2y, r2x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r1y, r1x, r2y, r1x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r2y, r1x, r2y, r2x, morgoth_margin))
            return false;

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
            else if (coord_in_morgoth_region(y, x, 1))
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
            else if (coord_in_morgoth_region(y, x, 1))
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

    /* Stairs can be placed anywhere on the map - rooms or corridors */
    
    /* Place "num" stairs */
    for (x = 0; x < num; x++)
    {
        int i;

        int yy, xx;

        for (i = 0; i < 1000; i++)
        {
            yy = rand_int(p_ptr->cur_map_hgt);
            xx = rand_int(p_ptr->cur_map_wid);

            /* make sure the square is empty (floor) and has no adjacent doors */
            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx))
                if ((cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD)
                    && (cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD))
                {
                    break;
                }
        }
        
        /* Failed to find valid location after 1000 attempts */
        if (i == 1000)
        {
            log_trace("alloc_stairs: Failed to find valid location for stair %d/%d after 1000 attempts", x+1, num);
            return (false);
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
        chance += 10 * bonus_traps;   /* +10/20/30 Ã”Ã‡aª on top of normal */

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

    /* Small caves (CA-blob partitions): sprinkle a few extra traps on open cave floors. */
    if (p_ptr->depth >= 8 && cave_clean_bold(y, x) && !(cave_info[y][x] & CAVE_ICKY)
        && (level_partition_kind_for_point(y, x) == LEVEL_PART_CAVEY))
    {
        chance = MAX(chance, 2);
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
    alloc_object_global(ALLOC_SET_BOTH, ALLOC_TYP_RUBBLE, r, false);

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

static bool connectivity_rescue_traversable(int ry, int rx)
{
    if (!in_bounds_fully(ry, rx))
        return false;

    if (cave_feat[ry][rx] == FEAT_WALL_PERM)
        return false;

    bool is_wall = (cave_feat[ry][rx] >= FEAT_WALL_HEAD)
        && (cave_feat[ry][rx] <= FEAT_WALL_TAIL)
        && (cave_feat[ry][rx] != FEAT_SECRET);

    /* Never carve through Morgoth's vault walls: require using the forced doors. */
    if (morgoth_level_active && (cave_info[ry][rx] & CAVE_G_VAULT) && is_wall)
        return false;

    /* Avoid carving new routes inside the sealed Morgoth region: only traverse
     * existing vault/tunnel squares there (and don't cross permanent walls). */
    if (coord_in_morgoth_region(ry, rx, 0)
        && !(cave_info[ry][rx] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
        && is_wall)
    {
        return false;
    }

    return true;
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

    /* Log which room centers are unreachable before rescue attempts */
    flood_access(p_ptr->py, p_ptr->px, cave_access, true);
    int unreachable_rooms = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        int ry = dun->cent[i].y;
        int rx = dun->cent[i].x;
        if (in_bounds_fully(ry, rx) && !cave_access[ry][rx])
        {
            unreachable_rooms++;
            genlog_connect("UNREACHABLE ROOM #%d at (%d,%d) bounds=(%d,%d)-(%d,%d)",
                           i, ry, rx, 
                           dun->corner[i].y1, dun->corner[i].x1,
                           dun->corner[i].y2, dun->corner[i].x2);
        }
    }
    if (unreachable_rooms > 0)
    {
        genlog_fail("PRE-RESCUE: %d/%d rooms unreachable from player at (%d,%d)",
                    unreachable_rooms, dun->cent_n, p_ptr->py, p_ptr->px);
    }
    
    /* Reset for rescue loop */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    /* Attempt connectivity with iterative rescue tunnels for each disconnected component */
    int rescue_attempts = 0;
    while (true)
    {
        // Make sure entire dungeon is connected (ignoring rubble and chasms)
        flood_access(p_ptr->py, p_ptr->px, cave_access, true);
        int unreachable = 0;
        int sample_y = -1, sample_x = -1;
        for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
                if (player_passable(y, x, true) && (cave_access[y][x] == false))
                {
                    unreachable++;
                    if (sample_y < 0)
                    {
                        sample_y = y;
                        sample_x = x;
                    }
                }

        if (unreachable == 0)
            break;

        /* Prefer sampling an unreachable room center to connect large components early. */
        if (dun)
        {
            for (int i = 0; i < dun->cent_n; ++i)
            {
                int ry = dun->cent[i].y;
                int rx = dun->cent[i].x;
                if (!in_bounds_fully(ry, rx)) continue;
                if (cave_access[ry][rx]) continue;
                if (!player_passable(ry, rx, true)) continue;
                sample_y = ry;
                sample_x = rx;
                break;
            }
        }

        /* Stop if we've tried too many rescues - scale with level size */
        /* Larger levels need more rescue attempts: base 20 + (blocks-8)*4 (and at least ~half room count). */
        int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
        int max_rescues = 20 + MAX(0, (blocks - 8) * 4);  /* 20 for 8 blocks, 72 for 21 blocks */
        if (dun) max_rescues = MAX(max_rescues, 20 + (dun->cent_n / 2));
        if (rescue_attempts++ >= max_rescues)
        {
            log_trace("check_connectivity: %d unreachable passable grids after %d rescues (first at %d,%d) -- FAILING",
                      unreachable, rescue_attempts, sample_y, sample_x);
            genlog_fail("CONNECTIVITY FAILED: %d unreachable passable grids after %d rescues (max=%d), first at (%d,%d)",
                        unreachable, rescue_attempts, max_rescues, sample_y, sample_x);
            return false;
        }

        /* Find the nearest reachable passable tile that would create a meaningful connection.
         * Prioritize tiles that are at least 4 tiles away to avoid useless short stubs
         * that just connect adjacent tiles on the same feature edge. */
        int best_y = -1, best_x = -1, best_d = 9999;
        int fallback_y = -1, fallback_x = -1, fallback_d = 9999;
        
        for (int yy = 1; yy < p_ptr->cur_map_hgt - 1; ++yy)
        {
            for (int xx = 1; xx < p_ptr->cur_map_wid - 1; ++xx)
            {
                if (!cave_access[yy][xx]) continue;
                if (!player_passable(yy, xx, true)) continue;
                if (coord_in_morgoth_region(yy, xx, 1)) continue;
                int d = ABS(yy - sample_y) + ABS(xx - sample_x);
                
                /* Prefer connections of distance >= 4 to avoid short useless stubs */
                if (d >= 4 && d < best_d)
                {
                    best_d = d;
                    best_y = yy;
                    best_x = xx;
                }
                /* Track fallback for any distance */
                if (d < fallback_d)
                {
                    fallback_d = d;
                    fallback_y = yy;
                    fallback_x = xx;
                }
            }
        }
        
        /* Use fallback if no good distance found */
        if (best_y < 0 || best_x < 0)
        {
            best_y = fallback_y;
            best_x = fallback_x;
            best_d = fallback_d;
        }

        if (best_y < 0 || best_x < 0)
        {
            log_trace("check_connectivity: no reachable target found for rescue (unreachable=%d)", unreachable);
            genlog_fail("CONNECTIVITY FAILED: no reachable target found for rescue (unreachable=%d), sample=(%d,%d)",
                        unreachable, sample_y, sample_x);
            return false;
        }

        /* Dig a rescue tunnel using BFS so we can route around permanent-wall obstacles. */
        {
            static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
            static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
            int head = 0, tail = 0;
            int found_y = -1, found_x = -1;

            /* Init prev array for current map bounds */
            for (int yy = 0; yy < p_ptr->cur_map_hgt; ++yy)
                for (int xx = 0; xx < p_ptr->cur_map_wid; ++xx)
                    prev[yy][xx] = -1;

            int start_y = sample_y, start_x = sample_x;
            int start_idx = start_y * MAX_DUNGEON_WID + start_x;
            prev[start_y][start_x] = start_idx; /* self */
            queue[tail++] = start_idx;

            static const int ddy4[4] = {-1, 1, 0, 0};
            static const int ddx4[4] = {0, 0, -1, 1};

            while (head < tail)
            {
                int cur = queue[head++];
                int cy = cur / MAX_DUNGEON_WID;
                int cx = cur % MAX_DUNGEON_WID;

                for (int d = 0; d < 4; ++d)
                {
                    int ny = cy + ddy4[d];
                    int nx = cx + ddx4[d];
                    if (!in_bounds_fully(ny, nx))
                        continue;
                    if (prev[ny][nx] != -1)
                        continue;
                    if (!connectivity_rescue_traversable(ny, nx))
                        continue;

                    int nidx = ny * MAX_DUNGEON_WID + nx;
                    prev[ny][nx] = cur;
                    if (tail < (int)N_ELEMENTS(queue))
                        queue[tail++] = nidx;

                    /* Found any reachable, passable tile */
                    if (cave_access[ny][nx] && player_passable(ny, nx, true))
                    {
                        found_y = ny;
                        found_x = nx;
                        head = tail; /* break out */
                        break;
                    }
                }
            }

            if (found_y < 0 || found_x < 0)
            {
                log_trace("check_connectivity: BFS rescue could not find a reachable target from (%d,%d)", sample_y, sample_x);
                genlog_fail("CONNECTIVITY FAILED: BFS rescue could not find reachable target from (%d,%d)", sample_y, sample_x);
                return false;
            }

            /* Carve path from found target back to sample */
            int carve_count = 0;
            int cur = found_y * MAX_DUNGEON_WID + found_x;
            int safety = 0;
            while (safety++ < (int)N_ELEMENTS(queue))
            {
                int cy = cur / MAX_DUNGEON_WID;
                int cx = cur % MAX_DUNGEON_WID;

                if (cave_feat[cy][cx] != FEAT_WALL_PERM)
                {
                    bool in_morgoth = coord_in_morgoth_region(cy, cx, 0);
                    bool allow_morgoth = (cave_info[cy][cx] & CAVE_MORGOTH_TUNNEL) != 0;

                    if (!in_morgoth || allow_morgoth)
                    {
                        if (!cave_floor_bold(cy, cx) && (cave_feat[cy][cx] < FEAT_DOOR_HEAD || cave_feat[cy][cx] > FEAT_DOOR_TAIL))
                        {
                            cave_set_feat(cy, cx, FEAT_FLOOR);
                            carve_count++;
                        }
                    }
                }

                if (cur == prev[start_y][start_x])
                    break;
                int p = prev[cy][cx];
                if (p == cur)
                    break;
                cur = p;
                if (cur == start_idx)
                    break;
            }

            log_trace("check_connectivity: BFS rescue tunnel from (%d,%d) to reachable (%d,%d), carved=%d (unreachable=%d, attempt=%d)",
                sample_y, sample_x, found_y, found_x, carve_count, unreachable, rescue_attempts);
            genlog_connect("RESCUE TUNNEL: BFS from (%d,%d) to (%d,%d), carved=%d",
                sample_y, sample_x, found_y, found_x, carve_count);
        }

        /* Clear and loop to re-check connectivity */
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
            for (x = 0; x < p_ptr->cur_map_wid; x++)
                cave_access[y][x] = false;
    }

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        return (true);
    }

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

    genlog_fail("CONNECTIVITY FAILED: player cannot reach down stairs without rubble/chasms");
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
    int stairs = 0;
    int initial_up = FEAT_LESS;
    int initial_down = FEAT_MORE;

    bool joined;
    bool no_down_stairs = (p_ptr->depth >= MORGOTH_DEPTH);

    /* Add backbone links across partition neighbors */
    connect_partition_hubs();

    // Phase 1:
    // connect each room to the closest room (if not already connected)
    // Try normal mode first, then desperate mode if that fails

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
            /* Try normal mode first, then desperate mode */
            if (!connect_two_rooms(r1, r_closest, true, false))
            {
                (void)connect_two_rooms(r1, r_closest, true, true);
            }
        }
    }
    
    // Phase 1.5: Connect to second-closest room as well for redundancy
    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        int closest1 = -1, closest2 = -1;
        int dist1 = 99999, dist2 = 99999;
        
        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 == r1) continue;
            d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
            if (d < dist1)
            {
                dist2 = dist1; closest2 = closest1;
                dist1 = d; closest1 = r2;
            }
            else if (d < dist2)
            {
                dist2 = d; closest2 = r2;
            }
        }
        
        /* Try to connect to second-closest if not already connected */
        if (closest2 >= 0 && !(dun->connection[r1][closest2]))
        {
            (void)connect_two_rooms(r1, closest2, true, false);
        }
    }

    // Phase 2:
    // make some random connections between rooms so long as they don't
    // intersect things

    switch (p_ptr->cur_map_hgt / PANEL_HGT)
    {
    case 3:
        corridor_attempts = dun->cent_n * dun->cent_n;
        break;
    case 4:
        corridor_attempts = dun->cent_n * dun->cent_n * 2;
        break;
    case 5:
    default:
        corridor_attempts = dun->cent_n * dun->cent_n * 10;
        break;
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

    /* Phase 3.5: L-shaped corridor fallback before force-connect.
     * Try carving clean L-shaped corridors between disconnected pieces.
     * This produces better-looking results than diagonal Bresenham carving. */
    if (pieces > 1)
    {
        int l_connects = 0;
        for (int attempt = 0; attempt < 100 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;
            
            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;
                    if (dun->connection[ra][rb])
                        continue;
                    
                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }
            
            if (best_a < 0 || best_b < 0)
                break;
            
            int y0 = dun->cent[best_a].y, x0 = dun->cent[best_a].x;
            int y1 = dun->cent[best_b].y, x1 = dun->cent[best_b].x;
            
            /* Try L-shaped corridor (horizontal then vertical, or vice versa) */
            bool carved = false;
            for (int dir = 0; dir < 2 && !carved; ++dir)
            {
                bool valid = true;
                
                /* Check if the L-path is carveable (no permanent walls) */
                int min_x = MIN(x0, x1), max_x = MAX(x0, x1);
                int min_y = MIN(y0, y1), max_y = MAX(y0, y1);
                
                /* Check horizontal leg */
                int leg_y = (dir == 0) ? y0 : y1;
                for (int tx = min_x; tx <= max_x && valid; ++tx)
                {
                    if (!in_bounds_fully(leg_y, tx) || cave_feat[leg_y][tx] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(leg_y, tx, 1))
                        valid = false;
                }
                
                /* Check vertical leg */
                int leg_x = (dir == 0) ? x1 : x0;
                for (int ty = min_y; ty <= max_y && valid; ++ty)
                {
                    if (!in_bounds_fully(ty, leg_x) || cave_feat[ty][leg_x] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(ty, leg_x, 1))
                        valid = false;
                }
                
                if (valid)
                {
                    /* Carve horizontal leg */
                    for (int tx = min_x; tx <= max_x; ++tx)
                    {
                        if (coord_in_morgoth_region(leg_y, tx, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(leg_y, tx))
                            cave_set_feat(leg_y, tx, FEAT_FLOOR);
                    }
                    if (!valid) continue;
                    /* Carve vertical leg */
                    for (int ty = min_y; ty <= max_y; ++ty)
                    {
                        if (coord_in_morgoth_region(ty, leg_x, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(ty, leg_x))
                            cave_set_feat(ty, leg_x, FEAT_FLOOR);
                    }
                    if (!valid) continue;
                    
                    dun->connection[best_a][best_b] = true;
                    dun->connection[best_b][best_a] = true;
                    carved = true;
                    l_connects++;
                }
            }
            
            pieces = dungeon_pieces();
        }
        
        if (l_connects > 0)
            log_trace("connect_rooms_stairs: L-shaped fallback carved %d connections, pieces now %d", l_connects, pieces);
    }

    /* Last resort: forcibly connect distinct pieces by digging a straight corridor
     * ignoring tunnel safety checks (but respecting permanent walls). This handles
     * adjacent-but-unconnected rooms/vaults seen on dense maps.
     * IMPROVED: Instead of picking random pairs, find the NEAREST pair of rooms
     * from different pieces to minimize ugly cross-map tunnels. */
    if (pieces > 1)
    {
        for (int attempt = 0; attempt < 50 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;
            
            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;
                    
                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }
            
            if (best_a < 0 || best_b < 0)
                break;  /* No valid pair found */
            
            int a = best_a;
            int b = best_b;

            int y0 = dun->cent[a].y, x0 = dun->cent[a].x;
            int y1 = dun->cent[b].y, x1 = dun->cent[b].x;

            log_trace("force-connect: linking room %d (piece %d) to room %d (piece %d), dist=%d",
                      a, dun->piece[a], b, dun->piece[b], best_dist);

            /* Bresenham carve that ignores h/v tunnel constraints */
            int dy = ABS(y1 - y0), sx = (x0 < x1) ? 1 : -1;
            int dx = ABS(x1 - x0), sy = (y0 < y1) ? 1 : -1;
            int err = (dx > dy ? dx : -dy) / 2;
            int y = y0, x = x0;
            bool aborted = false;
            while (true)
            {
                if (coord_in_morgoth_region(y, x, 1))
                {
                    aborted = true;
                    break;
                }
                if (in_bounds_fully(y, x) && cave_feat[y][x] != FEAT_WALL_PERM)
                {
                    if (!cave_floor_bold(y, x))
                        cave_set_feat(y, x, FEAT_FLOOR);
                }
                if (y == y1 && x == x1) break;
                int e2 = err;
                if (e2 > -dx) { err -= dy; x += sx; }
                if (e2 < dy)  { err += dx; y += sy; }
            }

            if (!aborted)
            {
                dun->connection[a][b] = dun->connection[b][a] = true;
                pieces = dungeon_pieces();
            }
        }

        log_trace("connect_rooms_stairs: forced-connect phase reduced pieces to %d", pieces);
    }

    // label_rooms();

    /* Calculate number of stairs based on map size: 2 for 66x66, 8 for 165x165 */
    /* Linear interpolation: stairs = 2 + (size - 66) * (8 - 2) / (165 - 66) */
    int map_size = (p_ptr->cur_map_hgt + p_ptr->cur_map_wid) / 2;  /* Average dimension */
    int stairs_max_base = 8;
    int stairs_max_total = 12;
    if (more_stairs)
    {
        stairs_max_base *= 2;
        stairs_max_total *= 2;
    }
    stairs = 2 + ((map_size - 66) * 6) / 99;  /* 6 = (8-2), 99 = (165-66) */
    if (stairs < 2) stairs = 2;   /* Minimum 2 */
    if (stairs > stairs_max_base) stairs = stairs_max_base;  /* Maximum 8 (or doubled) */
    
    /* Labyrinth bonus: +1 stair per labyrinth partition (more escape routes in mazes) */
    if (current_labyrinth_partitions > 0)
    {
        int stair_bonus = current_labyrinth_partitions;
        stairs += stair_bonus;
        log_trace("Labyrinth stair bonus: +%d stairs from %d labyrinth partitions (total=%d)",
                  stair_bonus, current_labyrinth_partitions, stairs);
    }

    if (more_stairs)
    {
        stairs += (stairs + 1) / 2; /* +50% (rounded up) */
    }
    if (stairs > stairs_max_total) stairs = stairs_max_total;
    
    log_trace("Map size %d leads to %d stairs each direction", map_size, stairs);

    /* Determine partition count for guaranteed stair placement */
    int partition_count = (map_size <= 80) ? 2 : 3;  /* Reduced from 4/9 to match lower stair count */
    int grid_rows = 1;
    int grid_cols = partition_count;
    if (partition_count == 4)
    {
        grid_rows = 2;
        grid_cols = 2;
    }
    else if (partition_count == 9)
    {
        grid_rows = 3;
        grid_cols = 3;
    }

    /* Place guaranteed stairs: at least one up and one down per partition */
    int down_placed = 0;
    int up_placed = 0;
    
    /* First pass: place one of each type per partition */
    for (int pi = 0; pi < partition_count; ++pi)
    {
        int row = pi / grid_cols;
        int col = pi % grid_cols;
        
        int y1 = 1 + (row * p_ptr->cur_map_hgt / grid_rows);
        int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 1;
        int x1 = 1 + (col * p_ptr->cur_map_wid / grid_cols);
        int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 1;
        
        /* Clamp boundaries */
        if (y2 >= p_ptr->cur_map_hgt - 1) y2 = p_ptr->cur_map_hgt - 2;
        if (x2 >= p_ptr->cur_map_wid - 1) x2 = p_ptr->cur_map_wid - 2;
        
        /* Place one down stair in this partition (unless final level) */
        if (!no_down_stairs)
        {
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                int yy = rand_range(y1, y2);
                int xx = rand_range(x1, x2);
                
                if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                    cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                    cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
                {
                    int feat = (p_ptr->on_the_run) ? FEAT_MORE_SHAFT : 
                              (down_placed == 0 || p_ptr->depth >= MORGOTH_DEPTH) ? FEAT_MORE : 
                              choose_down_stairs();
                    cave_set_feat(yy, xx, feat);
                    down_placed++;
                    break;
                }
            }
        }
        
        /* Place one up stair in this partition */
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            int yy = rand_range(y1, y2);
            int xx = rand_range(x1, x2);
            
            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
            {
                int feat = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT :
                          (up_placed == 0 || !p_ptr->depth) ? FEAT_LESS :
                          choose_up_stairs();
                cave_set_feat(yy, xx, feat);
                up_placed++;
                break;
            }
        }
    }
    
    log_trace("Guaranteed partition stairs: %d down, %d up placed", down_placed, up_placed);

    /* Second pass: place remaining stairs randomly across the map */
    int down_remaining = no_down_stairs ? 0 : (stairs - down_placed);
    int up_remaining = stairs - up_placed;
    
    /* Place remaining down stairs */
    int down_stairs = down_remaining;
    if (p_ptr->on_the_run)
        down_stairs *= 2;
    if ((p_ptr->create_stair == FEAT_MORE) || (p_ptr->create_stair == FEAT_MORE_SHAFT))
        down_stairs--;
    
    initial_down = p_ptr->on_the_run ? FEAT_MORE_SHAFT : FEAT_MORE;
    
    if (no_down_stairs)
        down_stairs = 0;

    if (down_stairs > 0 && !(alloc_stairs(initial_down, down_stairs)))
    {
        if (cheat_room)
            msg_format("Failed to place remaining down stairs.");
        log_trace("connect_rooms_stairs failed: Could not place %d remaining down stairs", down_stairs);
        return (false);
    }

    /* Place remaining up stairs */
    int up_stairs = up_remaining;
    if (p_ptr->on_the_run && p_ptr->depth >= 2)
        up_stairs *= 2;
    if ((p_ptr->create_stair == FEAT_LESS) || (p_ptr->create_stair == FEAT_LESS_SHAFT))
        up_stairs--;
    
    initial_up = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT : FEAT_LESS;
    
    if (up_stairs > 0 && !(alloc_stairs(initial_up, up_stairs)))
    {
        if (cheat_room)
            msg_format("Failed to place remaining up stairs.");
        log_trace("connect_rooms_stairs failed: Could not place %d remaining up stairs", up_stairs);
        return (false);
    }
    
    log_trace("Total stairs placed: %d down, %d up", down_placed + down_stairs, up_placed + up_stairs);

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
            {
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode(drop_mode_for_point(y0, x0));
                place_object_with_profile_params(y0, x0, object_level, object_level,
                    DROP_QUALITY_NORMAL, DROP_TYPE_CHEST, false, 1, 0, &active_profile);
            }
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
        if ((r_ptr->d_char == c)
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
 * Vault drop frequency gating — controls how many items spawn per vault symbol.
 * Driven by op_ptr->vault_drop_frequency (VDF_NORMAL..VDF_PLENTIFUL).
 */
typedef enum vault_drop_gate_kind {
    VDG_NORMAL = 0,
    VDG_GOOD,
    VDG_GREAT,
    VDG_CHEST
} vault_drop_gate_kind;

static int vault_drop_gate_percent(vault_drop_gate_kind kind)
{
    switch (op_ptr->vault_drop_frequency)
    {
    case VDF_PLENTIFUL:
        return 100;
    case VDF_NORMAL:
        switch (kind)
        {
        case VDG_NORMAL: return 40;
        case VDG_GOOD:   return 66;
        case VDG_GREAT:  return 100;
        case VDG_CHEST:  return 100;
        }
        break;
    case VDF_MODEST:
        switch (kind)
        {
        case VDG_NORMAL: return 20;
        case VDG_GOOD:   return 50;
        case VDG_GREAT:  return 75;
        case VDG_CHEST:  return 100;
        }
        break;
    case VDF_SCARCE:
        switch (kind)
        {
        case VDG_NORMAL: return 10;
        case VDG_GOOD:   return 25;
        case VDG_GREAT:  return 40;
        case VDG_CHEST:  return 66;
        }
        break;
    case VDF_MEAGER:
        switch (kind)
        {
        case VDG_NORMAL: return 0;
        case VDG_GOOD:   return 10;
        case VDG_GREAT:  return 20;
        case VDG_CHEST:  return 33;
        }
        break;
    }

    return 100;
}

static bool vault_drop_passes(vault_drop_gate_kind kind)
{
    int chance = vault_drop_gate_percent(kind);

    if (chance <= 0)
        return false;
    if (chance >= 100)
        return true;

    return percent_chance(chance);
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

            // chasms can't occur at 1000 ft
            if ((*t == '7') && (p_ptr->depth >= MORGOTH_DEPTH))
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
    /* If vault has explicit style list, use it (support '*'=-1, '$'=-2) */
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
        /* No S: provided -- choose a random style from the depth-available list */
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

            /* An object from 1-5 levels deeper (min-depth penalty only) */
            case '*':
            {
                /* Vault loot tuning: reduce item clutter based on drop frequency setting */
                if (!vault_drop_passes(VDG_NORMAL))
                    break;

                int base_depth = (p_ptr->depth > 0) ? p_ptr->depth : 1;
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode(drop_mode_for_point(y, x));
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_NORMAL,
                    DROP_TYPE_NOT_DAMAGED, false, 1, 0, &active_profile);
                break;
            }

            /* A good object from 1-5 levels deeper (min-depth penalty only) */
            case '&':
            {
                if (!vault_drop_passes(VDG_GOOD))
                    break;

                int base_depth = (p_ptr->depth > 0) ? p_ptr->depth : 1;
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode(drop_mode_for_point(y, x));
                bool old_allow_noble_from_quality = drop_allow_noble_from_quality;
                drop_allow_noble_from_quality
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_GOOD,
                    DROP_TYPE_NOT_DAMAGED, false, 1, 0, &active_profile);
                drop_allow_noble_from_quality = old_allow_noble_from_quality;
                break;
            }

            /* A great object from 1-5 levels deeper (min-depth penalty only) */
            case '!':
            {
                if (!vault_drop_passes(VDG_GREAT))
                    break;

                int base_depth = (p_ptr->depth > 0) ? p_ptr->depth : 1;
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode(drop_mode_for_point(y, x));
                bool old_allow_noble_from_quality = drop_allow_noble_from_quality;
                drop_allow_noble_from_quality
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_GREAT,
                    DROP_TYPE_NOT_DAMAGED, true, 10, IDENT_HOARD_DROP, &active_profile);
                drop_allow_noble_from_quality = old_allow_noble_from_quality;
                break;
            }

            /* A chest from 5 levels deeper */
            case '~':
            {
                if (!vault_drop_passes(VDG_CHEST))
                    break;

                int chest_depth;
                if (p_ptr->depth == 0)
                    chest_depth = MORGOTH_DEPTH;
                else
                    chest_depth = p_ptr->depth + 5;

                /* Set vault type context for chest material distribution */
                drop_set_chest_vault_type(v_ptr->typ);
                
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode(drop_mode_for_point(y, x));
                place_object_with_profile_params(
                    y, x, chest_depth, chest_depth, DROP_QUALITY_NORMAL,
                    DROP_TYPE_CHEST, false, 1, 0, &active_profile);
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

            /* A human skeleton */
            case 'h':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                /* Get local object */
                i_ptr = &object_type_body;

                /* Wipe the object */
                object_wipe(i_ptr);

                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);

                /* Prepare the item */
                object_prep(i_ptr, k_idx);

                i_ptr->pval = 1;

                /* Drop it in the dungeon */
                drop_near(i_ptr, -1, y, x);
                break;
            }

            /* An orc skeleton */
            case 'e':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                /* Get local object */
                i_ptr = &object_type_body;

                /* Wipe the object */
                object_wipe(i_ptr);

                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);

                /* Prepare the item */
                object_prep(i_ptr, k_idx);

                i_ptr->pval = 1;

                /* Drop it in the dungeon */
                drop_near(i_ptr, -1, y, x);
                break;
            }

            /* A web */
            case 'w':
            {
                /* Place a web trap */
                cave_set_feat(y, x, FEAT_TRAP_WEB);
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
                    /* Vault loot tuning: reduce item clutter based on drop frequency setting */
                    if (!vault_drop_passes(VDG_NORMAL))
                        break;

                    int base_depth = (p_ptr->depth > 0) ? p_ptr->depth : 1;
                    int penalty_depth = base_depth + 1;
                    partition_drop_profile active_profile =
                        partition_drop_profile_for_mode(drop_mode_for_point(y, x));
                    place_object_with_profile_params(
                        y, x, base_depth, penalty_depth, DROP_QUALITY_NORMAL,
                        DROP_TYPE_UNTHEMED, false, 1, 0, &active_profile);
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
                /* 5% chance for alert thrall (with quest), 95% for dejected thrall */
                int thrall_r_idx;
                if (one_in_(20))
                {
                    /* Alert thrall with quest */
                    thrall_r_idx = one_in_(2) ? R_IDX_ALERT_HUMAN_THRALL : R_IDX_ALERT_ELF_THRALL;
                }
                else
                {
                    /* Dejected thrall (no quest) */
                    thrall_r_idx = one_in_(2) ? R_IDX_HUMAN_THRALL : R_IDX_ELF_THRALL;
                }
                place_monster_one(y, x, thrall_r_idx, true, true, NULL);
                
                /* Initialize quest for alert thralls */
                if (thrall_r_idx == R_IDX_ALERT_HUMAN_THRALL || thrall_r_idx == R_IDX_ALERT_ELF_THRALL)
                {
                    int m_idx = cave_m_idx[y][x];
                    if (m_idx > 0)
                    {
                        init_thrall_quest(&mon_list[m_idx]);
                    }
                }
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

            /* Troll (any monster with RF3_TROLL) */
            case 't':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_TROLL, true, p_ptr->depth + rand_range(1, 4));
                break;
            }

            /* The Mail Corslet of Durin (INSTA_ART; vault-only) */
            case 'u':
            {
                create_chosen_artefact(ART_DURIN, y, x, false);
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

            /* Wight/Wraith */
            case 'g':
            {
                place_monster_by_letter(
                    y, x, 'W', true, p_ptr->depth + rand_range(1, 4));
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

            /* Ancalagon the Black */
            case 'K':
            {
                place_vault_monster_token('K', y, x);
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

    /* Never dock Morgoth's throne room */
    if (v_ptr->typ == 9)
    {
        return false;
    }

    if (v_ptr->flags & (VLT_QUEST))
    {
        return false;
    }

    if (dun->cent_n >= room_capacity_limit())
    {
        return false;
    }

    /* Collect existing vault indices to target */
    int vault_indices[CENT_MAX];
    int vault_count = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (room_kind_is_vault(dun->kind[i]) && !dun->is_quest[i] && dun->kind[i] != 9)
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

            /* Carve through walls in BOTH vaults to ensure passability.
             * We need to carve into the docked vault AND into the base vault,
             * since either side may have thick walls at the contact point. */
            int dy = 0, dx = 0;
            if (dir == VAULT_DOCK_EAST) dx = 1;
            else if (dir == VAULT_DOCK_WEST) dx = -1;
            else if (dir == VAULT_DOCK_SOUTH) dy = 1;
            else dy = -1;

            /* Carve in both directions from the door */
            for (int side = 0; side < 2; ++side)
            {
                int carve_dy, carve_dx, start_y, start_x;
                
                if (side == 0)
                {
                    /* Carve into the docked vault */
                    carve_dy = dy;
                    carve_dx = dx;
                    start_y = new_y;
                    start_x = new_x;
                }
                else
                {
                    /* Carve into the base vault (opposite direction) */
                    carve_dy = -dy;
                    carve_dx = -dx;
                    start_y = contact_y - dy;
                    start_x = contact_x - dx;
                }
                
                int carve_y = start_y;
                int carve_x = start_x;
                int max_carve = 6;
                bool found_floor = false;
                
                for (int c = 0; c < max_carve; ++c)
                {
                    int feat = cave_feat[carve_y][carve_x];
                    if (feat == FEAT_FLOOR || feature_is_any_door(feat))
                    {
                        found_floor = true;
                        break;
                    }
                    if (!(cave_info[carve_y][carve_x] & CAVE_ICKY))
                        break;
                    cave_set_feat(carve_y, carve_x, FEAT_FLOOR);
                    carve_y += carve_dy;
                    carve_x += carve_dx;
                }
                
                /* If straight carve didn't find floor, search perpendicular */
                if (!found_floor)
                {
                    int perp_dy = (carve_dy == 0) ? 1 : 0;
                    int perp_dx = (carve_dx == 0) ? 1 : 0;
                    int search_radius = 8;
                    
                    for (int sign = -1; sign <= 1; sign += 2)
                    {
                        for (int dist = 1; dist <= search_radius; ++dist)
                        {
                            int check_y = carve_y + sign * perp_dy * dist;
                            int check_x = carve_x + sign * perp_dx * dist;
                            
                            if (!(cave_info[check_y][check_x] & CAVE_ICKY))
                                break;
                            
                            int feat = cave_feat[check_y][check_x];
                            if (feat == FEAT_FLOOR || feature_is_any_door(feat))
                            {
                                for (int d = 1; d < dist; ++d)
                                {
                                    int path_y = carve_y + sign * perp_dy * d;
                                    int path_x = carve_x + sign * perp_dx * d;
                                    cave_set_feat(path_y, path_x, FEAT_FLOOR);
                                }
                                found_floor = true;
                                break;
                            }
                        }
                        if (found_floor) break;
                    }
                }
            }

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
            && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
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
            && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
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

        /* Surface vaults get exponentially rarer at depth */
        {
            int rarity = v_ptr->rarity;
            if ((p_ptr->depth >= 6) && (v_ptr->flags & (VLT_SURFACE)))
            {
                rarity += (1 << p_ptr->depth);
            }

            /* Accept the first greater vault (but not quest vaults) */
            if ((v_ptr->typ == 8) && (v_ptr->depth <= p_ptr->depth)
                && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
                && (one_in_(rarity)) && !(v_ptr->flags & VLT_QUEST))
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
static bool build_type9(int y0, int x0, vault_type** used_vault)
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

    if (used_vault)
        *used_vault = v_ptr;

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

/* Carve two 3-wide tunnels from the north face of the throne room up toward the partition edge */
static void carve_morgoth_entry_tunnels(const vault_type* v_ptr, int y0, int x0)
{
    if (!v_ptr)
        return;

    int top_y = y0 - v_ptr->hgt / 2;
    int left_x = x0 - v_ptr->wid / 2;
    int right_x = left_x + v_ptr->wid - 1;

    /* Collect contiguous '$' runs on the top row (stored as FEAT_WALL_OUTER) */
    int seg_start[4];
    int seg_end[4];
    int segs = 0;

    for (int x = left_x; x <= right_x; x++)
    {
        if (cave_feat[top_y][x] == FEAT_WALL_OUTER)
        {
            if (segs == 0 || x != seg_end[segs - 1] + 1)
            {
                if (segs >= 4)
                    break;
                seg_start[segs] = seg_end[segs] = x;
                segs++;
            }
            else
            {
                seg_end[segs - 1] = x;
            }
        }
    }

    if (segs == 0)
        return;

    int tunnel_limit = morgoth_partition_reserved ? morgoth_partition_bounds.y1 - 2 : top_y - 20;
    if (tunnel_limit < 1)
        tunnel_limit = 1;
    if (tunnel_limit > top_y)
        tunnel_limit = top_y;

    /* Track which segments have joined independently */
    bool seg_joined[4] = {false, false, false, false};

    for (int s = 0; s < segs; s++)
    {
        int x1 = seg_start[s];
        int x2 = seg_end[s];

        /* Place forced closed doors in the vault's outer wall (end of corridor) */
        for (int x = x1; x <= x2; x++)
        {
            if (!in_bounds_fully(top_y, x))
                continue;
            cave_set_feat(top_y, x, FEAT_DOOR_HEAD + 0x00);
        }

        /* Carve a tunnel northwards from just outside the doors */
        for (int y = top_y - 1; y >= tunnel_limit; y--)
        {
            /* Skip if this segment already joined */
            if (seg_joined[s])
                break;

            bool this_seg_joined = false;

            for (int x = x1; x <= x2; x++)
            {
                if (!in_bounds_fully(y, x))
                    continue;

                /* Stop this segment once it reaches existing open floor outside the reserved region */
                if (!morgoth_region_active() || !coord_in_morgoth_region(y, x, 0))
                {
                    if (cave_floor_bold(y, x) && !(cave_info[y][x] & CAVE_ICKY))
                    {
                        this_seg_joined = true;
                        continue;
                    }
                }

                if (cave_feat[y][x] == FEAT_WALL_PERM)
                    continue;

                cave_set_feat(y, x, FEAT_FLOOR);
                cave_info[y][x] &= ~(CAVE_G_VAULT | CAVE_ICKY);
                cave_info[y][x] |= CAVE_MORGOTH_TUNNEL;
            }

            if (this_seg_joined)
            {
                seg_joined[s] = true;
                break;
            }
        }
    }
}

/* Extend the carved entry tunnels so both connect to the main level. */
static bool morgoth_tunnel_traversable(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_WALL_PERM)
        return false;
    if ((cave_info[y][x] & CAVE_ICKY) && !coord_in_morgoth_region(y, x, 0))
        return false;
    return true;
}

static bool morgoth_tunnel_target(int y, int x)
{
    if (coord_in_morgoth_region(y, x, 0))
        return false;
    if (cave_info[y][x] & CAVE_ICKY)
        return false;
    return player_passable(y, x, true);
}

static void connect_morgoth_tunnel_component(int start_y, int start_x)
{
    static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int head = 0;
    int tail = 0;
    int found_y = -1;
    int found_x = -1;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            prev[y][x] = -1;

    int start_idx = start_y * MAX_DUNGEON_WID + start_x;
    prev[start_y][start_x] = start_idx;
    queue[tail++] = start_idx;

    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    while (head < tail)
    {
        int cur = queue[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 4; ++d)
        {
            int ny = cy + ddy4[d];
            int nx = cx + ddx4[d];
            if (!in_bounds_fully(ny, nx))
                continue;
            if (prev[ny][nx] != -1)
                continue;
            if (!morgoth_tunnel_traversable(ny, nx))
                continue;

            int nidx = ny * MAX_DUNGEON_WID + nx;
            prev[ny][nx] = cur;
            if (tail < (int)N_ELEMENTS(queue))
                queue[tail++] = nidx;

            if (morgoth_tunnel_target(ny, nx))
            {
                found_y = ny;
                found_x = nx;
                head = tail;
                break;
            }
        }
    }

    if (found_y < 0 || found_x < 0)
        return;

    int cur = found_y * MAX_DUNGEON_WID + found_x;
    int safety = 0;
    while (safety++ < (int)N_ELEMENTS(queue))
    {
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        if (cave_feat[cy][cx] != FEAT_WALL_PERM)
        {
            if (!cave_floor_bold(cy, cx)
                && (cave_feat[cy][cx] < FEAT_DOOR_HEAD
                    || cave_feat[cy][cx] > FEAT_DOOR_TAIL))
            {
                cave_set_feat(cy, cx, FEAT_FLOOR);
            }

            if (coord_in_morgoth_region(cy, cx, 0))
                cave_info[cy][cx] |= CAVE_MORGOTH_TUNNEL;
        }

        if (cur == prev[start_y][start_x])
            break;
        int p = prev[cy][cx];
        if (p == cur)
            break;
        cur = p;
        if (cur == start_idx)
            break;
    }
}

static void cave_set_feat_style(int y, int x, int feat, int style_idx)
{
    if (style_idx >= 0)
        cave_set_feat_with_color(y, x, feat, style_idx);
    else
        cave_set_feat(y, x, feat);
}

static bool place_monster_by_flag_try(int y, int x, int flagset, u32b flag, bool allow_unique, int max_depth)
{
    if (cave_m_idx[y][x] != 0)
        return false;
    place_monster_by_flag(y, x, flagset, flag, allow_unique, max_depth);
    return (cave_m_idx[y][x] != 0);
}

static bool place_monster_by_letter_try(int y, int x, char letter, bool allow_unique, int max_depth)
{
    int tries = 0;
    int depth = max_depth;

    while (depth > 0)
    {
        int r_idx = get_mon_num(depth, false, true, true);
        monster_race* r_ptr = &r_info[r_idx];

        if (r_ptr->d_char == letter
            && (allow_unique || !(r_ptr->flags1 & (RF1_UNIQUE))))
        {
            return place_monster_one(y, x, r_idx, true, false, NULL);
        }

        tries++;
        if (tries >= 100)
        {
            tries = 0;
            depth--;
        }
    }

    return false;
}

static bool place_big_cave_elemental_monster(int y, int x, big_cave_type_t cave_type, int max_depth)
{
    if (cave_type == BIG_CAVE_FIRE)
    {
        if (place_monster_by_flag_try(y, x, 4, RF4_BRTH_FIRE, true, max_depth))
            return true;
        return place_monster_by_flag_try(y, x, 3, RF3_RES_FIRE, true, max_depth);
    }
    if (cave_type == BIG_CAVE_ICE)
    {
        if (place_monster_by_flag_try(y, x, 4, RF4_BRTH_COLD, true, max_depth))
            return true;
        return place_monster_by_flag_try(y, x, 3, RF3_RES_COLD, true, max_depth);
    }
    if (cave_type == BIG_CAVE_POIS)
    {
        if (place_monster_by_flag_try(y, x, 4, RF4_BRTH_POIS, true, max_depth))
            return true;
        return place_monster_by_flag_try(y, x, 3, RF3_RES_POIS, true, max_depth);
    }
    return false;
}

static bool place_big_cave_troll_or_giant(int y, int x, int max_depth)
{
    if (one_in_(2))
    {
        if (place_monster_by_flag_try(y, x, 3, RF3_TROLL, true, max_depth))
            return true;
        return place_monster_by_letter_try(y, x, 'G', true, max_depth);
    }
    if (place_monster_by_letter_try(y, x, 'G', true, max_depth))
        return true;
    return place_monster_by_flag_try(y, x, 3, RF3_TROLL, true, max_depth);
}

static void connect_morgoth_entry_tunnels(void)
{
    if (!morgoth_region_active())
        return;

    static bool visited[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            visited[y][x] = false;

    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!(cave_info[y][x] & CAVE_MORGOTH_TUNNEL))
                continue;
            if (visited[y][x])
                continue;

            int head = 0;
            int tail = 0;
            int min_y = y;
            int min_x = x;
            int max_x = x;
            bool start_found = false;

            int start_idx = y * MAX_DUNGEON_WID + x;
            queue[tail++] = start_idx;
            visited[y][x] = true;

            while (head < tail)
            {
                int cur = queue[head++];
                int cy = cur / MAX_DUNGEON_WID;
                int cx = cur % MAX_DUNGEON_WID;

                if (cy < min_y)
                {
                    min_y = cy;
                    min_x = cx;
                    max_x = cx;
                    start_found = true;
                }
                else if (cy == min_y)
                {
                    if (!start_found)
                    {
                        min_x = cx;
                        max_x = cx;
                        start_found = true;
                    }
                    else
                    {
                        min_x = MIN(min_x, cx);
                        max_x = MAX(max_x, cx);
                    }
                }

                for (int d = 0; d < 4; ++d)
                {
                    int ny = cy + ddy4[d];
                    int nx = cx + ddx4[d];
                    if (!in_bounds_fully(ny, nx))
                        continue;
                    if (visited[ny][nx])
                        continue;
                    if (!(cave_info[ny][nx] & CAVE_MORGOTH_TUNNEL))
                        continue;
                    visited[ny][nx] = true;
                    if (tail < (int)N_ELEMENTS(queue))
                        queue[tail++] = ny * MAX_DUNGEON_WID + nx;
                }
            }

            int start_y = min_y;
            int start_x = (min_x + max_x) / 2;
            if (!(cave_info[start_y][start_x] & CAVE_MORGOTH_TUNNEL))
            {
                bool found = false;
                for (int tx = min_x; tx <= max_x; ++tx)
                {
                    if (cave_info[start_y][tx] & CAVE_MORGOTH_TUNNEL)
                    {
                        start_x = tx;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    start_x = x;
            }

            connect_morgoth_tunnel_component(start_y, start_x);
        }
    }
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
#if 0
static bool room_build(int typ)
{
    int y, x;

    if (dun->cent_n >= room_capacity_limit())
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
#endif

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
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;

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
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
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
            if (cave_feat[y][x] == FEAT_FLOOR
                && !((y == p_ptr->py) && (x == p_ptr->px)))
                cave_set_feat(y, x, FEAT_RUBBLE);
            for (n = (y - 1); n <= (y + 1); n++)
            {
                for (m = (x - 1); m <= (x + 1); m++)
                {
                    if ((n == p_ptr->py) && (m == p_ptr->px))
                        continue;
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

static bool varda_sunlight_tile_ok(int y, int x, bool require_empty)
{
    if (!in_bounds_fully(y, x)) return false;
    if (cave_feat[y][x] != FEAT_SUNLIGHT) return false;
    if (!cave_floor_bold(y, x)) return false;
    if (cave_info[y][x] & CAVE_ICKY) return false;
    if (require_empty && cave_m_idx[y][x] != 0) return false;

    return true;
}

static void varda_make_sunlight_pool(int y, int x)
{
    for (int ny = y - 1; ny <= y + 1; ny++)
    {
        for (int nx = x - 1; nx <= x + 1; nx++)
        {
            if (!in_bounds_fully(ny, nx)) continue;
            if (cave_info[ny][nx] & CAVE_ICKY) continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR && cave_feat[ny][nx] != FEAT_RAGE_FLOOR
                && cave_feat[ny][nx] != FEAT_SUNLIGHT) continue;
            cave_set_feat(ny, nx, FEAT_SUNLIGHT);
        }
    }
}

static bool varda_no_rubble_path_tile_ok(int y, int x,
    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    if (!access[y][x]) return false;

    /* Avoid spawning her adjacent to the player (quest can auto-trigger before encounter XP). */
    if (distance(p_ptr->py, p_ptr->px, y, x) < 2) return false;

    return true;
}

/*
 * Pick a sunlight tile suitable for spawning Varda:
 * - empty
 * - reachable from the player without digging rubble / crossing chasms
 * - not adjacent to the player
 *
 * Returns the number of spawnable sunlight tiles found (0 if none).
 * Optionally returns:
 * - total sunlight tiles (occupied or not)
 * - empty sunlight tiles (regardless of reachability)
 */
static int pick_varda_sunlight_spawn_tile(int *out_y, int *out_x,
    int *out_total_sunlight, int *out_empty_sunlight)
{
    int total = 0;
    int empty = 0;
    int spawnable = 0;
    int pick_y = -1;
    int pick_x = -1;

    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            access[y][x] = false;
        }
    }
    flood_access(p_ptr->py, p_ptr->px, access, false);

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (!varda_sunlight_tile_ok(y, x, false)) continue;
            total++;

            if (!varda_sunlight_tile_ok(y, x, true)) continue;
            empty++;

            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
            spawnable++;
            if (one_in_(spawnable)) {
                pick_y = y;
                pick_x = x;
            }
        }
    }

    if (out_total_sunlight) *out_total_sunlight = total;
    if (out_empty_sunlight) *out_empty_sunlight = empty;
    if (spawnable > 0 && out_y && out_x) {
        *out_y = pick_y;
        *out_x = pick_x;
    }

    return spawnable;
}

static bool force_varda_sunlight_tile(int *out_y, int *out_x)
{
    int count = 0;
    int pick_y = -1;
    int pick_x = -1;

    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            access[y][x] = false;
        }
    }
    flood_access(p_ptr->py, p_ptr->px, access, false);

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
        {
            if (cave_info[y][x] & CAVE_ICKY) continue;
            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
            if (!cave_empty_bold(y, x)) continue;
            if (cave_feat[y][x] != FEAT_FLOOR && cave_feat[y][x] != FEAT_RAGE_FLOOR) continue;

            count++;
            if (one_in_(count)) {
                pick_y = y;
                pick_x = x;
            }
        }
    }

    if (count == 0) return false;

    varda_make_sunlight_pool(pick_y, pick_x);
    if (out_y) {
        *out_y = pick_y;
        *out_x = pick_x;
    }

    return true;
}

static void ensure_sunlight_for_varda(void)
{
    /* Only relevant for the first few levels */
    if (p_ptr->depth > 3) return;
    
    /* Check for valid sunlight spawn locations */
    int total_sunlight = 0;
    int empty_sunlight = 0;
    int spawnable_sunlight = pick_varda_sunlight_spawn_tile(NULL, NULL, &total_sunlight, &empty_sunlight);
    
    if (spawnable_sunlight == 0) {
        log_trace("Varda spawn: No valid sunlight spawn locations detected (total=%d, empty=%d), seeding patches",
            total_sunlight, empty_sunlight);
        make_patches_of_sunlight();
        
        /* Verify at least one valid location exists after patching */
        total_sunlight = 0;
        empty_sunlight = 0;
        spawnable_sunlight = pick_varda_sunlight_spawn_tile(NULL, NULL, &total_sunlight, &empty_sunlight);
        
        if (spawnable_sunlight > 0) {
            log_trace("Varda spawn: Verified sunlight after patching (total=%d, empty=%d, spawnable=%d)",
                total_sunlight, empty_sunlight, spawnable_sunlight);
            return;
        }

        int forced_y = -1;
        int forced_x = -1;
        if (force_varda_sunlight_tile(&forced_y, &forced_x)) {
            log_trace("Varda spawn: Forced sunlight at (%d,%d) to guarantee spawn", forced_y, forced_x);
            return;
        }

        log_trace("Varda spawn: WARNING - No valid sunlight locations after patching or forcing!");
    }
}

/*
 * Generate a new dungeon level
 */
static bool cave_gen(void)
{
    int i;

    int l;

    int y, x;

    int room_attempts = 0;

    int is_guaranteed_forge_level = false;
    bool duruin_bastion_forced = false;
    bool is_morgoth_level = (p_ptr->depth == MORGOTH_DEPTH);

    reset_morgoth_layout_state(is_morgoth_level);
    
    /* Reset labyrinth partition counter for this level */
    current_labyrinth_partitions = 0;
    
    /* Reset quest vault monitoring variables for this level */
    qv_placed_this_level = false;
    qv_stored_y1 = qv_stored_x1 = qv_stored_y2 = qv_stored_x2 = -1;
    
    /* Run quest lottery once per level to determine which quest (if any) gets this level */
    if (is_morgoth_level) {
        quest_lottery_winner = 0;
    } else {
        run_quest_lottery();
    }
    
    /* Debug: Log entry into cave_gen */
    log_trace("cave_gen: Starting level generation (quest_vault_used=%s, lottery_winner=%d)", 
              p_ptr->quest_vault_used ? "true" : "false", quest_lottery_winner);
    
    /* Varda quest reserves the run to avoid other quest content until complete */
    if (!is_morgoth_level && p_ptr->varda_quest >= VARDA_QUEST_ACTIVE && !p_ptr->quest_reserved[0]) {
        p_ptr->quest_reserved[0] = 1;
        log_trace("Varda quest: === QUEST SLOT RESERVED === Active Varda quest reserves slot (state=%d)", p_ptr->varda_quest);
    }
    
    log_trace("cave_gen: Quest status at level start - quest_reserved[0]=%d, varda_quest=%d, lottery_winner=%d",
              p_ptr->quest_reserved[0], p_ptr->varda_quest, quest_lottery_winner);
    
    /* Varda quest: flag forced bastion placement on first level deeper than 500ft */
    if (!is_morgoth_level && p_ptr->varda_quest == VARDA_QUEST_ACTIVE && !p_ptr->varda_vault_placed && p_ptr->depth > 10) {
        if (!p_ptr->varda_vault_ready) {
            log_trace("Varda quest: Crossing 500ft, setting bastion_ready at depth %d", p_ptr->depth);
        }
        p_ptr->varda_vault_ready = 1;
    }
    s16b mon_gen, obj_room_gen;
    memset(dun, 0, sizeof(*dun));

    /* Sil - determine the dungeon size */
    /* Generate square levels: 4*11 to 15*11 (44x44 to 165x165) */
    /* Probability increases with depth, larger sizes more probable */
    
    // Base size: 9 blocks (increased from 7 for larger level sizes)
    // Size increases with depth, with bias toward larger sizes
    // Formula: Use multiple dice rolls and take the maximum (biases upward)
    // Two independent uniform rolls: X1 = dieroll(17) (1..17), X2 = dieroll(14) (1..14)
    int base_size = 9;  // Increased from 7 for larger starting levels
    int depth_factor = p_ptr->depth + dieroll(17);  // Higher ceiling (1-17)
    int bonus1 = depth_factor / 3;  // First roll (uses X1)
    int bonus2 = (p_ptr->depth + dieroll(14)) / 3;  // Second roll (uses X2)
    int depth_bonus = MAX(bonus1, bonus2);  // Take maximum (biases larger)
    
    l = base_size + depth_bonus;
    if (l > MAX_LEVEL_BLOCKS) l = MAX_LEVEL_BLOCKS;  // Hard cap at MAX_LEVEL_BLOCKS
    if (l < 8) l = 8;    // Hard floor at 8 blocks (88x88)

    if (smaller_level_size)
    {
        l -= 3;
        if (l < 6) l = 6; /* Allow 6x6 and 7x7 block maps */
    }

    // Square levels: same dimension for both height and width
    p_ptr->cur_map_hgt = l * (PANEL_HGT);
    p_ptr->cur_map_wid = l * (PANEL_HGT);  // Use PANEL_HGT for both to make square

    /* Fewer room attempts to reduce long regen loops; vault bias handled later */
    room_attempts = l * l * l * 2;
    log_trace("cave_gen: SQUARE map size set to %dx%d (l=%d blocks) room_attempts=%d", 
              p_ptr->cur_map_wid, p_ptr->cur_map_hgt, l, room_attempts);
    
    /* Generation log: level start */
    gen_log_level_start(p_ptr->depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
    genlog_summary("Level %d generation starting: %dx%d map (%d blocks), %d room attempts",
                   p_ptr->depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid, l, room_attempts);
    genlog_quest("Quest lottery winner=%d, quest_vault_used=%s, varda_quest=%d",
                 quest_lottery_winner, p_ptr->quest_vault_used ? "yes" : "no", p_ptr->varda_quest);

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

    // guarantee a forge at first entrance to levels 2, 6, 10 (or below if skipped via shaft)
    if (p_ptr->fixed_forge_count < 3)
    {
        int next_guaranteed_forge_level = 2 + (p_ptr->fixed_forge_count * 4);
        is_guaranteed_forge_level = (next_guaranteed_forge_level <= p_ptr->depth);
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

    if (!is_morgoth_level)
    {
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
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 18), could not place type 8/7/6 - regenerating", quest_vault_roll);
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                }
                else if (quest_vault_roll >= 13)
                {
                    log_trace("Quest vault: Hit lesser vault threshold (%d >= 13), trying quest vaults 7->6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(7) || try_quest_vault_type(6);
                    
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 13), could not place type 7/6 - regenerating", quest_vault_roll);
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                }
                else if (quest_vault_roll >= 8)
                {
                    log_trace("Quest vault: Hit interesting room threshold (%d >= 8), trying quest vault 6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(6);
                    
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 8), could not place type 6 - regenerating", quest_vault_roll);
                        gen_log_level_end(false, dun->cent_n, 1);
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
    }

    /* Seed a handful of prefab anchors up front to diversify layout */
    seed_prefab_anchors();
    /* Apply quadrant generation modes - this is now the primary room generation */
    apply_quadrant_generation_modes();
    /* DISABLED: ensure_partition_connectivity() was creating dead-end corridors.
     * The corridor system and rescue tunnels handle connectivity instead. */
    /* Repair all outer walls - critical fix for tunnel connectivity after overlapping generation */
    repair_all_outer_walls();

    /* Verify Morgoth's throne room was placed (should have been done in apply_quadrant_generation_modes) */
    if (morgoth_level_active && !morgoth_partition_reserved)
    {
        log_trace("Morgoth level: throne room was not placed during partition generation");
        return false;
    }

    /* Room saturation loop DISABLED - partition system handles room generation
     * The old approach saturated the map with random rooms which conflicted with
     * the partition-based generation that already creates themed areas. */
#if 0
    /* Build some rooms */
    int failed_in_row = 0;
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

        // choose a room type based on the level (bias toward vaults)
        if ((r < 4) || one_in_(3))
        {
            // standard room
            log_trace("Room generation: Building standard room (r=%d)", r);
            if (!room_build(1))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if (r < 7)
        {
            // cross room
            log_trace("Room generation: Building cross room (r=%d)", r);
            if (!room_build(2))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if ((r < 14) || one_in_(2))
        {
            // interesting room
            log_trace("Room generation: Building interesting room (r=%d)", r);
            if (!room_build(6))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if (r < 19)
        {
            // lesser vault
            log_trace("Room generation: Building lesser vault (r=%d)", r);
            if (!room_build(7))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else
        {
            // greater vault
            log_trace("Room generation: Building greater vault (r=%d)", r);
            if (!room_build(8))
                failed_in_row++;
            else
                failed_in_row = 0;
        }

        // stop if there are too many rooms
        if (dun->cent_n >= room_capacity_limit())
            break;

        // bail out if we are not making progress to avoid infinite loops
        if (failed_in_row > 200)
        {
            log_trace("Room generation: aborting after %d consecutive failures (cent_n=%d)", failed_in_row, dun->cent_n);
            break;
        }
    }
#endif

    /*set the permanent walls*/
    set_perm_boundry();

    /* Post-partition seeders DISABLED - partition system already handles these
     * CA blob and BSP slice anchors were duplicating work the partitions do */
#if 0
    /* Carve CA blob anchors into remaining granite */
    seed_ca_blob_anchors();
    /* Add BSP-slice anchors for rectangular-but-offset caverns */
    seed_bsp_slice_anchors();
#endif

    /* If generation stalled, force a couple of simple rooms to avoid regen loops */
    ensure_minimum_rooms();

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
        genlog_fail("NOT ENOUGH ROOMS: %d generated, minimum %d required", dun->cent_n, ROOM_MIN);
        gen_log_level_end(false, dun->cent_n, 1);
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
        genlog_fail("CONNECTIVITY FAILED: connect_rooms_stairs() could not link rooms (rooms=%d)", dun->cent_n);
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* DEBUGGING: Check if quest vault still exists after tunnel making */
    check_quest_vault_integrity("AFTER_TUNNEL_GENERATION");

    if (morgoth_level_active)
        connect_morgoth_entry_tunnels();

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

    if (morgoth_level_active)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_MORE)
                    || (cave_feat[y][x] == FEAT_MORE_SHAFT))
                {
                    cave_set_feat(y, x, FEAT_LESS);
                }
            }
        }
    }

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
        genlog_fail("PLACEMENT FAILED: place_rubble_player() could not place stairs/player");
        gen_log_level_end(false, dun->cent_n, 1);
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

    /* Note: Labyrinth monsters are now placed directly inside the labyrinth
     * during carve_labyrinth_bounds() instead of as a global bonus */

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
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* Partition-based item allocation: count rooms per partition type */
    int roomy_rooms = 0, cavey_rooms = 0, ruined_rooms = 0;
    int labyrinth_rooms = 0, chasm_rooms = 0, big_cave_rooms = 0;
    
    for (i = 0; i < dun->cent_n; i++)
    {
        int cy = dun->cent[i].y;
        int cx = dun->cent[i].x;
        quadrant_mode_t mode = partition_mode_for_point(cy, cx);
        
        switch (mode)
        {
            case QUAD_MODE_ROOMY:     roomy_rooms++; break;
            case QUAD_MODE_CAVEY:     cavey_rooms++; break;
            case QUAD_MODE_RUINED:    ruined_rooms++; break;
            case QUAD_MODE_LABYRINTH: labyrinth_rooms++; break;
            case QUAD_MODE_CHASM:     chasm_rooms++; break;
            case QUAD_MODE_BIG_CAVE:  big_cave_rooms++; break;
        }
    }
    
    /* Calculate monsters per partition type (base rate: 0.5-1.0 per room) */
    int roomy_mon = (roomy_rooms + dieroll(roomy_rooms)) / 2;
    /* Non-ROOMY partitions use grp=false in alloc_monster (no BFS group spreading).
     * Multiply their counts to compensate for the suppressed group spawning:
     * chasm=x4 (very open platforms), cavey=x3 (open blobs), ruined=x2 (has walls),
     * labyrinth=x2 (narrow but wall-constrained), big_cave=x5 (widest open). */
    int cavey_mon    = ((cavey_rooms    + dieroll(MAX(1, cavey_rooms)))    / 2) * 3;
    int ruined_mon   = ((ruined_rooms   + dieroll(MAX(1, ruined_rooms)))   / 2) * 2;
    int labyrinth_mon = ((labyrinth_rooms + dieroll(MAX(1, labyrinth_rooms))) / 2) * 2;
    int chasm_mon    = ((chasm_rooms    + dieroll(MAX(1, chasm_rooms)))    / 2) * 4;
    int big_cave_mon = ((big_cave_rooms + dieroll(MAX(1, big_cave_rooms))) / 2) * 5;
    
    /* Apply meta-run curse to all partition types */
    {
        int stacks = curse_flag_count_cur(CUR_MON_NUM);
        if (stacks)
        {
            int mult = 100 + 30 * stacks;
            roomy_mon = roomy_mon * mult / 100;
            cavey_mon = cavey_mon * mult / 100;
            ruined_mon = ruined_mon * mult / 100;
            labyrinth_mon = labyrinth_mon * mult / 100;
            chasm_mon = chasm_mon * mult / 100;
            big_cave_mon = big_cave_mon * mult / 100;
        }
    }
    
    /* Partition-specific item allocation rates:
     * - Roomy: mon/8 rooms + mon/12 corridors (reduced from mon/4 + mon/6)
     * - Cavey: mon/2 rooms + mon/4 corridors (caves already have extra gems/mithril)
     * - Ruined: mon/2 rooms + mon/4 corridors (ruins already have skeletons)
     * - Labyrinth: mon/2 rooms + mon/4 corridors
     * - Chasm: mon/2 rooms + mon/4 corridors
     * - Big Cave: mon/2 rooms + mon/4 corridors */
     
    int roomy_room_obj = roomy_mon / 8;
    int roomy_corr_obj = roomy_mon / 12;
    int cavey_room_obj = cavey_mon / 2;
    int cavey_corr_obj = cavey_mon / 4;
    int ruined_room_obj = ruined_mon / 2;
    int ruined_corr_obj = ruined_mon / 4;
    int labyrinth_room_obj = labyrinth_mon / 2;
    int labyrinth_corr_obj = labyrinth_mon / 4;
    int chasm_room_obj = chasm_mon / 2;
    int chasm_corr_obj = chasm_mon / 4;
    int big_cave_room_obj = big_cave_mon / 2;
    int big_cave_corr_obj = big_cave_mon / 4;

    /* Scale floor/corridor item counts by vault drop frequency setting.
     * VDF_NORMAL (default) = 100%, so behaviour is unchanged at that setting. */
    {
        int pct;
        switch (op_ptr->vault_drop_frequency)
        {
        case VDF_PLENTIFUL: pct = 150; break;
        case VDF_NORMAL:    pct = 100; break;
        case VDF_MODEST:    pct =  67; break;
        case VDF_SCARCE:    pct =  33; break;
        case VDF_MEAGER:    pct =  10; break;
        default:            pct = 100; break;
        }
        if (pct != 100)
        {
#define SCALE_OBJ(x) x = MAX(0, (x) * pct / 100)
            SCALE_OBJ(roomy_room_obj);
            SCALE_OBJ(roomy_corr_obj);
            SCALE_OBJ(cavey_room_obj);
            SCALE_OBJ(cavey_corr_obj);
            SCALE_OBJ(ruined_room_obj);
            SCALE_OBJ(ruined_corr_obj);
            SCALE_OBJ(labyrinth_room_obj);
            SCALE_OBJ(labyrinth_corr_obj);
            SCALE_OBJ(chasm_room_obj);
            SCALE_OBJ(chasm_corr_obj);
            SCALE_OBJ(big_cave_room_obj);
            SCALE_OBJ(big_cave_corr_obj);
#undef SCALE_OBJ
        }
    }
    
    /* Total item counts for logging */
    obj_room_gen = roomy_room_obj + cavey_room_obj + ruined_room_obj + 
                   labyrinth_room_obj + chasm_room_obj + big_cave_room_obj;
    int obj_corr_gen = roomy_corr_obj + cavey_corr_obj + ruined_corr_obj + 
                       labyrinth_corr_obj + chasm_corr_obj + big_cave_corr_obj;
    
    /* Place objects in rooms - partition by partition */
    if (roomy_room_obj > 0)
        alloc_object_in_partition(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, roomy_room_obj, QUAD_MODE_ROOMY);
    if (cavey_room_obj > 0)
        alloc_object_in_partition(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, cavey_room_obj, QUAD_MODE_CAVEY);
    if (ruined_room_obj > 0)
        alloc_object_in_partition(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, ruined_room_obj, QUAD_MODE_RUINED);
    if (labyrinth_room_obj > 0)
        alloc_object_in_partition(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, labyrinth_room_obj, QUAD_MODE_LABYRINTH);
    if (chasm_room_obj > 0)
        alloc_object_in_partition(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, chasm_room_obj, QUAD_MODE_CHASM);
    if (big_cave_room_obj > 0)
        alloc_object_in_partition(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, big_cave_room_obj, QUAD_MODE_BIG_CAVE);
    
    log_trace("Room objects: %d total (%d roomy, %d cavey, %d ruined, %d lab, %d chasm, %d bigcave)",
              obj_room_gen, roomy_room_obj, cavey_room_obj, ruined_room_obj,
              labyrinth_room_obj, chasm_room_obj, big_cave_room_obj);
    
    /* Place objects in corridors - partition by partition */
    if (roomy_corr_obj > 0)
        alloc_object_in_partition(ALLOC_SET_CORR, ALLOC_TYP_OBJECT, roomy_corr_obj, QUAD_MODE_ROOMY);
    if (cavey_corr_obj > 0)
        alloc_object_in_partition(ALLOC_SET_CORR, ALLOC_TYP_OBJECT, cavey_corr_obj, QUAD_MODE_CAVEY);
    if (ruined_corr_obj > 0)
        alloc_object_in_partition(ALLOC_SET_CORR, ALLOC_TYP_OBJECT, ruined_corr_obj, QUAD_MODE_RUINED);
    if (labyrinth_corr_obj > 0)
        alloc_object_in_partition(ALLOC_SET_CORR, ALLOC_TYP_OBJECT, labyrinth_corr_obj, QUAD_MODE_LABYRINTH);
    if (chasm_corr_obj > 0)
        alloc_object_in_partition(ALLOC_SET_CORR, ALLOC_TYP_OBJECT, chasm_corr_obj, QUAD_MODE_CHASM);
    if (big_cave_corr_obj > 0)
        alloc_object_in_partition(ALLOC_SET_CORR, ALLOC_TYP_OBJECT, big_cave_corr_obj, QUAD_MODE_BIG_CAVE);
    
    log_trace("Corridor objects: %d total (%d roomy, %d cavey, %d ruined, %d lab, %d chasm, %d bigcave)",
              obj_corr_gen, roomy_corr_obj, cavey_corr_obj, ruined_corr_obj,
              labyrinth_corr_obj, chasm_corr_obj, big_cave_corr_obj);
    
    /* Recalculate total mon_gen for monster placement (sum of all partitions) */
    mon_gen = roomy_mon + cavey_mon + ruined_mon + labyrinth_mon + chasm_mon + big_cave_mon;

    // place the traps
    place_traps();

    /* Put some monsters in the dungeon */
    for (i = mon_gen; i > 0; i--)
    {
        (void)alloc_monster(false, false);
    }

    /* Top-up monsters in partitions that tend to have few room centers (big caves, chasms, etc). */
    int extra_mon = place_partition_extra_monsters();
    if (extra_mon > 0)
        log_trace("Extra partition monsters placed: %d", extra_mon);
    
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
            genlog_quest("VARDA SPAWN FAILED: depth %d > 3, forcing regeneration", p_ptr->depth);
            gen_log_level_end(false, dun->cent_n, 1);
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

             int try_y = -1;
             int try_x = -1;
             int total_sunlight = 0;
            int empty_sunlight = 0;
            int spawnable_sunlight = pick_varda_sunlight_spawn_tile(&try_y, &try_x, &total_sunlight, &empty_sunlight);

            log_trace("Varda spawn: Sunlight tiles total=%d, empty=%d, spawnable=%d",
                total_sunlight, empty_sunlight, spawnable_sunlight);

            if (spawnable_sunlight == 0) {
                log_trace("Varda spawn: No spawnable sunlight tiles available, forcing a sunlit tile");
                if (force_varda_sunlight_tile(&try_y, &try_x)) {
                    spawnable_sunlight = 1;
                }
            }

            if (spawnable_sunlight > 0) {
                if (place_monster_one(try_y, try_x, R_IDX_VARDA, true, true, NULL)) {
                    varda_spawned = true;
                } else {
                    log_trace("Varda spawn: Primary sunlight tile rejected, scanning for fallback");

                    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
                    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
                    {
                        for (int x = 0; x < p_ptr->cur_map_wid; x++)
                        {
                            access[y][x] = false;
                        }
                    }
                    flood_access(p_ptr->py, p_ptr->px, access, false);

                    for (int y = 1; y < p_ptr->cur_map_hgt - 1 && !varda_spawned; y++) {
                        for (int x = 1; x < p_ptr->cur_map_wid - 1 && !varda_spawned; x++) {
                            if (!varda_sunlight_tile_ok(y, x, true)) continue;
                            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
                            if (place_monster_one(y, x, R_IDX_VARDA, true, true, NULL)) {
                                try_y = y;
                                try_x = x;
                                varda_spawned = true;
                            }
                        }
                    }
                }
            }

            if (varda_spawned) {
                p_ptr->varda_quest = VARDA_QUEST_GIVER_PRESENT;
                p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                p_ptr->varda_level = p_ptr->depth;
                log_trace("Varda spawn: === SUCCESS === Placed at (%d,%d) on sunlight tile", try_y, try_x);
                log_trace("Varda spawn: Quest state set to GIVER_PRESENT (%d), quest_reserved[0]=1", p_ptr->varda_quest);
            }
            
            if (!varda_spawned)
            {
                log_trace("Varda spawn: === FAILED === Could not find valid sunlight tile after forcing - REGENERATING LEVEL");
                genlog_fail("VARDA SPAWN FAILED: could not find valid sunlight tile after forcing - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
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
                    
                    /* Must be valid coordinates, floor in the same room, and not too close to player */
                    if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                        try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(try_y, try_x) && 
                        (cave_info[try_y][try_x] & CAVE_ROOM) &&
                        !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                        cave_m_idx[try_y][try_x] == 0 &&
                        distance(player_y, player_x, try_y, try_x) >= 2)
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
            genlog_quest("NIENA SPAWN FAILED: level size %d < 5, forcing regeneration", l);
            gen_log_level_end(false, dun->cent_n, 1);
            return false; /* Force regeneration until we get a big enough level */
        }
        
        /* Check stair distance requirement: must be at least 87 grid distance */
        int min_stair_dist = calculate_min_stair_distance();
        log_trace("Niena spawn: Calculated minimum stair distance = %d", min_stair_dist);
        
        if (min_stair_dist < 87) {
            log_trace("Niena spawn: FAILED - stairs too close (distance=%d, need >=87)", min_stair_dist);
            genlog_quest("NIENA SPAWN FAILED: stair distance %d < 87, forcing regeneration", min_stair_dist);
            gen_log_level_end(false, dun->cent_n, 1);
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
                    
                    /* Must be valid coordinates, floor in the same room, and not too close to player */
                    if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                        try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(try_y, try_x) && 
                        (cave_info[try_y][try_x] & CAVE_ROOM) &&
                        !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                        cave_m_idx[try_y][try_x] == 0 &&
                        distance(player_y, player_x, try_y, try_x) >= 2 &&
                        los(player_y, player_x, try_y, try_x))
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
                genlog_fail("NIENA SPAWN FAILED: could not place monster after all attempts - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
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

    /* Check for Orome quest spawning - only if it won the lottery */
    int orome_completions = metarun_quest_completion_count(METARUN_QUEST_OROME);
    bool orome_blocked = quest_metarun_blocked(QUEST_ID_OROME, METARUN_QUEST_OROME);
    log_trace("Orome spawn check: quest=%d, depth=%d, metarun_completions=%d, lottery_winner=%d, blocked=%s", 
             p_ptr->orome_quest, p_ptr->depth, 
             orome_completions,
             quest_lottery_winner,
             orome_blocked ? "yes" : "no");
             
    /* Only attempt Orome spawning if it won the lottery and isn't blocked by metarun history */
    if (orome_blocked) {
        log_trace("Orome spawn: blocked by metarun state (requires active oath or under cap)");
        quest_lottery_winner = 0; /* Treat level as quest-free if history blocks this quest */
    } else if (quest_lottery_winner == 5) { /* Orome is quest ID 5 */
        log_trace("Orome spawn: Orome WON the lottery - attempting spawn");
        
        /* Try to find a room to spawn Orome in */
        int attempts;
        bool orome_spawned = false;
        
        log_trace("Orome spawn: Lottery winner attempting placement at depth %d", p_ptr->depth);
        
        /* Check if Orome already exists on this level */
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
            /* Try to spawn Orome near the player's starting room */
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
                
                /* Must be valid coordinates, floor in the same room, and not too close to player */
                if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                    try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                    cave_floor_bold(try_y, try_x) && 
                    (cave_info[try_y][try_x] & CAVE_ROOM) &&
                    !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                    cave_m_idx[try_y][try_x] == 0 &&
                    distance(player_y, player_x, try_y, try_x) >= 2 &&
                    los(player_y, player_x, try_y, try_x))
                {
                    if (place_monster_one(try_y, try_x, R_IDX_OROME, true, true, NULL))
                    {
                        p_ptr->orome_quest = OROME_QUEST_GIVER_PRESENT;
                        p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
                        orome_spawned = true;
                        log_trace("Orome spawned near player at (%d, %d), player at (%d, %d), quest state: %d", 
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
                            log_trace("Orome spawned in fallback room at (%d, %d), quest state: %d", 
                                     room_y, room_x, p_ptr->orome_quest);
                        }
                    }
                }
            }
            
            if (!orome_spawned)
            {
                log_trace("Orome spawn: FAILED - could not place monster after 150 attempts");
                genlog_fail("OROME SPAWN FAILED: could not place monster after 150 attempts - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
                return false; /* Force regeneration */
            }
        }
        else
        {
            log_trace("Orome already exists on level, skipping room spawn");
        }
    } else {
        log_trace("Orome spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    // place Morgoth if on the run
    if (p_ptr->on_the_run && !p_ptr->morgoth_slain)
    {
        bool placed = false;
        int sils = silmarils_possessed();
        int max_dist = 50 - (sils * 8);
        int min_dist = 9 - sils;

        if (max_dist < min_dist + 2)
            max_dist = min_dist + 2;

        /* Prefer spawning within a chase radius scaled by Silmarils. */
        for (int pass = 0; pass < 2 && !placed; ++pass)
        {
            bool require_no_los = (pass == 0);

            for (i = 0; i <= 180; i++)
            {
                int dy = rand_range(-max_dist, max_dist);
                int dx = rand_range(-max_dist, max_dist);
                int dist = ABS(dy) + ABS(dx);

                if (dist < min_dist || dist > max_dist)
                    continue;

                y = p_ptr->py + dy;
                x = p_ptr->px + dx;

                if (!in_bounds_fully(y, x))
                    continue;
                if (!cave_empty_bold(y, x))
                    continue;
                if (cave_info[y][x] & (CAVE_ICKY))
                    continue;
                if (require_no_los && los(p_ptr->py, p_ptr->px, y, x))
                    continue;

                if (place_monster_one(y, x, R_IDX_MORGOTH, false, true, NULL))
                {
                    placed = true;
                    break;
                }
            }
        }

        if (!placed)
        {
            for (y = 1; y < p_ptr->cur_map_hgt - 1 && !placed; ++y)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1 && !placed; ++x)
                {
                    if (!cave_empty_bold(y, x))
                        continue;
                    if (cave_info[y][x] & (CAVE_ICKY))
                        continue;

                    if (place_monster_one(y, x, R_IDX_MORGOTH, false, true, NULL))
                        placed = true;
                }
            }
        }

        if (placed && cave_m_idx[y][x] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            if (m_ptr->r_idx == R_IDX_MORGOTH)
            {
                if (m_ptr->alertness < ALERTNESS_ALERT)
                    m_ptr->alertness = ALERTNESS_ALERT;
                m_ptr->min_range = 0;
            }
        }
        else if (!placed)
        {
            log_trace("Morgoth spawn: FAILED to place Morgoth while on the run (depth=%d)", p_ptr->depth);
        }
    }
    p_ptr->force_forge = false;

    /* Level generation successful - log completion */
    genlog_summary("Level %d generation COMPLETE: %d rooms, quest_lottery=%d",
                   p_ptr->depth, dun->cent_n, quest_lottery_winner);
    gen_log_level_end(true, dun->cent_n, 1);
    gen_log_flush();

    return (true);
}

/*
 * Create the gates to Angband level
 */
static void gates_gen(void)
{
    int y, x;
    int i;
    int py = -1, px = -1;

    memset(dun, 0, sizeof(*dun));
    layout_anchor_reset();
    reset_morgoth_layout_state(false);
    current_partition_rows = 0;
    current_partition_cols = 0;
    current_partition_count = 0;
    current_labyrinth_partitions = 0;
    for (i = 0; i < PARTITION_META_MAX; ++i)
    {
        current_partition_modes[i] = QUAD_MODE_ROOMY;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
    }

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

    if (!build_type10(17, 33))
    {
        log_error("gates_gen: failed to build Gates of Angband vault");
    }

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

    if ((py < 0) || (px < 0))
    {
        msg_format("Failed to find a down staircase in the gates level");
        py = p_ptr->cur_map_hgt / 2;
        px = p_ptr->cur_map_wid / 2;
        for (y = py - 1; y <= py + 1; ++y)
        {
            for (x = px - 1; x <= px + 1; ++x)
            {
                if (!in_bounds(y, x))
                    continue;
                cave_set_feat(y, x, FEAT_FLOOR);
            }
        }
        cave_set_feat(py, px, FEAT_MORE);
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
#if 0
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

    build_type9(16, 38, NULL);

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
#endif

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
    bool is_morgoth_level = (p_ptr->depth == MORGOTH_DEPTH);

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

    /* Never carry the throne-room truce between levels */
    p_ptr->truce = false;

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

        /* Build a real level */
        else
        {
            /* Make a dungeon, or report the failure to make one*/
            if (cave_gen())
            {
                okay = true;
                if (is_morgoth_level)
                {
                    /* Depth 20 uses the partition system; keep entry stairs so the player can retreat. */
                }
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

    /* Reset per-level skeleton note limits once the layout is finalized */
    skeleton_note_level_reset();

    /* Tag all tiles belonging to chasm partitions (whole partition bounds). */
    apply_chasm_partition_tags();

    /* Enforce partition/room lighting rules (e.g. labyrinth/CA_BLOB always dark). */
    apply_partition_and_room_glow_rules();

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
