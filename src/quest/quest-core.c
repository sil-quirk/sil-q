#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "quest/quest-internal.h"

/*
 * Select a suitable unique monster for the Tulkas quest
 */
static bool tulkas_target_valid(int r_idx, const monster_race* r_ptr, int depth)
{
    if (!r_ptr) return false;

    if (!(r_ptr->flags1 & RF1_UNIQUE)) return false;
    if (r_ptr->max_num <= 0) return false;
    if (r_ptr->level < depth) return false;
    if (r_ptr->level > MORGOTH_DEPTH) return false;
    if (r_idx == R_IDX_TULKAS || r_idx == R_IDX_MORGOTH) return false;

    return true;
}

static bool tulkas_has_valid_target(int depth)
{
    int i;

    if (!z_info) return false;

    for (i = 1; i < z_info->r_max; i++)
    {
        if (tulkas_target_valid(i, &r_info[i], depth)) return true;
    }

    return false;
}

int select_tulkas_quest_target(void)
{
    int i;
    int valid_targets[50];
    int count = 0;

    log_trace("select_tulkas_quest_target: z_info=%p, r_max=%d", z_info, z_info ? z_info->r_max : -1);

    if (!z_info)
    {
        log_trace("z_info is NULL!");
        return 0;
    }

    /* Look for unique monsters at current depth or deeper */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];

        if (!r_ptr)
        {
            log_trace("r_info[%d] returned NULL", i);
            continue;
        }

        /* Must be unique, alive (max_num > 0), and at appropriate depth */
        /* Exclude Tulkas himself and Morgoth from being targets */
        if (tulkas_target_valid(i, r_ptr, p_ptr->depth))
        {
            valid_targets[count] = i;
            count++;
            if (count >= 50) break; /* Safety limit */
        }
    }

    if (count == 0) {
        log_trace("select_tulkas_quest_target: No valid unique targets found");
        return 0; /* No valid targets */
    }

    log_trace("select_tulkas_quest_target: Found %d valid unique targets", count);
    return valid_targets[rand_int(count)];
}

/*
 * Select a suitable artifact prize for the Tulkas quest
 */
int select_tulkas_quest_prize(int target_level)
{
    int i;
    int valid_prizes[100];
    int count = 0;
    int max_artifact_level = target_level + 6; /* Not more than 6 levels deeper */

    log_trace("select_tulkas_quest_prize: target_level=%d, max_artifact_level=%d, z_info=%p, art_max=%d",
              target_level, max_artifact_level, z_info, z_info ? z_info->art_max : -1);

    if (!z_info)
    {
        log_trace("z_info is NULL!");
        return 0;
    }

    if (!valar_reserved_artifacts)
    {
        log_trace("valar_reserved_artifacts is NULL! Initializing...");

        /* Initialize the array if it doesn't exist */
        if (z_info && z_info->art_max > 0) {
            valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
            for (int j = 0; j < z_info->art_max; j++) {
                valar_reserved_artifacts[j] = false;
            }
            log_trace("Initialized valar_reserved_artifacts with %d entries", z_info->art_max);
        } else {
            log_error("Cannot initialize valar_reserved_artifacts: z_info=%p, art_max=%d",
                     z_info, z_info ? z_info->art_max : -1);
            return 0;
        }
    }

    /* First pass: Look for artifacts with rarity >= 10 within depth constraint */
    for (i = 1; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        if (!a_ptr)
        {
            log_trace("a_info[%d] returned NULL", i);
            continue;
        }

        /* Must be high rarity, within depth constraint, and not yet created */
        if ((a_ptr->rarity >= 10) &&
            (a_ptr->level >= target_level) &&
            (a_ptr->level <= max_artifact_level) &&
            (a_ptr->cur_num == 0) &&
            !valar_reserved_artifacts[i])
        {
            valid_prizes[count] = i;
            count++;
            if (count >= 100) break; /* Safety limit */
        }
    }

    /* If no suitable artifacts found with rarity >= 10, increase level requirement */
    if (count == 0)
    {
        log_trace("No artifacts found with rarity >= 10 within depth constraint, relaxing requirements");
        max_artifact_level = MORGOTH_DEPTH; /* Remove depth constraint */

        /* Second pass: Look for any artifacts with rarity >= 10 regardless of depth */
        for (i = 1; i < z_info->art_max; i++)
        {
            artefact_type* a_ptr = &a_info[i];

            if (!a_ptr) continue;

            /* Must be high rarity, appropriate level, and not yet created */
            if ((a_ptr->rarity >= 10) &&
                (a_ptr->level >= target_level) &&
                (a_ptr->cur_num == 0) &&
                !valar_reserved_artifacts[i])
            {
                valid_prizes[count] = i;
                count++;
                if (count >= 100) break; /* Safety limit */
            }
        }

        /* Third pass: If still no artifacts, take highest level artifact available */
        if (count == 0)
        {
            log_trace("No artifacts found with rarity >= 10, looking for highest level artifact");
            int best_artifact = 0;
            int best_level = 0;

            for (i = 1; i < z_info->art_max; i++)
            {
                artefact_type* a_ptr = &a_info[i];

                if (!a_ptr) continue;

                /* Must be high rarity and not yet created, ignore level constraint */
                if ((a_ptr->rarity >= 10) &&
                    (a_ptr->cur_num == 0) &&
                    !valar_reserved_artifacts[i] &&
                    (a_ptr->level > best_level))
                {
                    best_artifact = i;
                    best_level = a_ptr->level;
                }
            }

            if (best_artifact > 0)
            {
                valid_prizes[0] = best_artifact;
                count = 1;
                log_trace("Selected highest level artifact: %d (level %d)", best_artifact, best_level);
            }
        }

        if (count > 0)
        {
            log_trace("Found %d artifacts after relaxing depth constraint", count);
        }
    }
    else
    {
        log_trace("Found %d artifacts with rarity >= 10 within depth constraint", count);
    }

    if (count == 0) return 0; /* No valid prizes */

    return valid_prizes[rand_int(count)];
}

/*
 * Get metarun quest flag from quest index by looking up the M: field in quest.txt
 * Returns 0 if quest has no metarun tracking or quest_idx is invalid
 */
u32b get_metarun_quest_flag(int quest_idx)
{
    quest_type* q_ptr;
    const char* metarun_id;

    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return 0;

    q_ptr = &quest_info[quest_idx];

    /* Get the metarun quest ID string from the M: field */
    if (q_ptr->metarun_quest_id == 0) return 0;
    metarun_id = quest_name_text + q_ptr->metarun_quest_id;

    /* Map the string to the corresponding flag value */
    if (streq(metarun_id, "METARUN_QUEST_TULKAS")) return METARUN_QUEST_TULKAS;
    if (streq(metarun_id, "METARUN_QUEST_AULE")) return METARUN_QUEST_AULE;
    if (streq(metarun_id, "METARUN_QUEST_MANDOS")) return METARUN_QUEST_MANDOS;
    if (streq(metarun_id, "METARUN_QUEST_NIENA")) return METARUN_QUEST_NIENA;
    if (streq(metarun_id, "METARUN_QUEST_OROME")) return METARUN_QUEST_OROME;
    if (streq(metarun_id, "METARUN_QUEST_VARDA")) return METARUN_QUEST_VARDA;

    /* Unknown or future quest */
    log_debug("get_metarun_quest_flag: Unknown metarun_quest_id '%s' for quest_idx %d", metarun_id, quest_idx);
    return 0;
}

/*
 * Apply quest rewards (stats, skills, abilities) based on quest.txt data
 */
void apply_quest_rewards(int quest_idx)
{
    quest_type* q_ptr;

    /* Validate quest index */
    if (!p_ptr || quest_idx <= 0 || quest_idx >= z_info->quest_max) return;

    q_ptr = &quest_info[quest_idx];

    /* Apply stat bonuses */
    for (int i = 0; i < 4; i++) {
        if (q_ptr->stat_bonuses[i] > 0) {
            /* stat_bonuses array: [str, dex, con, gra] */
            int stat_idx = i; /* A_STR=0, A_DEX=1, A_CON=2, A_GRA=3 */

            for (int j = 0; j < q_ptr->stat_bonuses[i]; j++) {
                if (p_ptr->stat_base[stat_idx] < BASE_STAT_MAX) {
                    p_ptr->stat_base[stat_idx]++;
                }
            }

            log_trace("Applied %s bonus: +%d",
                     (i == 0 ? "STR" : i == 1 ? "DEX" : i == 2 ? "CON" : "GRA"),
                     q_ptr->stat_bonuses[i]);
        }
    }

    /* Apply skill bonus */
    if (q_ptr->skill_bonus > 0 && q_ptr->skill_type < S_MAX) {
        int old_skill = p_ptr->skill_base[q_ptr->skill_type];

        p_ptr->skill_base[q_ptr->skill_type] += q_ptr->skill_bonus;

        switch (q_ptr->skill_type) {
            case S_MEL:
                log_trace("Applied Melee bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_ARC:
                log_trace("Applied Archery bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_EVN:
                log_trace("Applied Evasion bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_STL:
                log_trace("Applied Stealth bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_PER:
                log_trace("Applied Perception bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_WIL:
                log_trace("Applied Will bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_SMT:
                log_trace("Applied Smithing bonus: +%d", q_ptr->skill_bonus);
                break;
            case S_SNG:
                log_trace("Applied Song bonus: +%d", q_ptr->skill_bonus);
                break;
            default:
                log_trace("Applied skill bonus: skill=%d bonus=+%d",
                          q_ptr->skill_type, q_ptr->skill_bonus);
                break;
        }

        if (old_skill == 0 && p_ptr->skill_base[q_ptr->skill_type] > 0
            && (q_ptr->skill_type == S_SNG
                || q_ptr->skill_type == S_SMT))
        {
            sdl_quick_access_suggest_skill_shortcut(q_ptr->skill_type);
        }
    }

    /* Apply special ability */
    if (q_ptr->ability_type > 0 && q_ptr->ability_id < ABILITIES_MAX) {
        /* ability_type 8 is the Special skill type (S_SPC) based on quest.txt */
        if (q_ptr->ability_type == 8) {
            /* Grant the special ability using the ability_id from quest.txt */
            if (!p_ptr->have_ability[S_SPC][q_ptr->ability_id]) {
                p_ptr->have_ability[S_SPC][q_ptr->ability_id] = true;
                p_ptr->innate_ability[S_SPC][q_ptr->ability_id] = true;
                p_ptr->active_ability[S_SPC][q_ptr->ability_id] = true;
                ability_log_record_gain(S_SPC, q_ptr->ability_id);

                /* Get the ability name for the message */
                ability_type* b_ptr = &b_info[ability_index(S_SPC, q_ptr->ability_id)];
                if (b_ptr && b_ptr->name && b_name) {
                    msg_format("You have learned %s!", b_name + b_ptr->name);
                    log_trace("Applied special ability: %s (skill=%d, ability=%d)",
                             b_name + b_ptr->name, q_ptr->ability_type, q_ptr->ability_id);
                } else {
                    msg_print("You have gained a new special ability!");
                    log_trace("Applied special ability: Unknown name (skill=%d, ability=%d)",
                             q_ptr->ability_type, q_ptr->ability_id);
                }
            } else {
                /* Already have this ability */
                ability_type* b_ptr = &b_info[ability_index(S_SPC, q_ptr->ability_id)];
                if (b_ptr && b_ptr->name && b_name) {
                    msg_format("You already possess %s.", b_name + b_ptr->name);
                } else {
                    msg_print("You already possess this special ability.");
                }
                log_trace("Special ability already granted: skill=%d, ability=%d",
                         q_ptr->ability_type, q_ptr->ability_id);
            }
        }
        else {
            log_trace("Unknown ability type: %d (not implemented)", q_ptr->ability_type);
        }
    }

    /* Recalculate bonuses and redraw */
    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_STATS);
}

/*
 * Get oath ID from quest data
 */
int get_quest_oath_id(int quest_idx)
{
    quest_type* q_ptr;

    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return 0; /* No oath */

    q_ptr = &quest_info[quest_idx];
    return q_ptr->oath_id;
}

/*
 * Check quest eligibility based on E: field data and standard quest requirements
 * Includes metarun completion checks and quest state checks
 */
bool check_quest_eligibility(int quest_idx, int depth)
{
    quest_type* q_ptr;

    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return false;

    q_ptr = &quest_info[quest_idx];

    /* Debug quest 2 specifically - show what was actually loaded */
    if (quest_idx == 2) {
        log_trace("Quest %d (Aulë) LOADED DATA: eligibility_type=%d, eligibility_skill=%d, eligibility_value=%d",
                 quest_idx, q_ptr->eligibility_type, q_ptr->eligibility_skill, q_ptr->eligibility_value);
        log_trace("Quest %d (Aulë) LOADED DATA: This data was loaded from save file, not parsed from quest.txt", quest_idx);
    }

    /* Check standard requirements that apply to all quests */
    /* 1. Quest not already completed in current metarun unless oath override applies */
    u32b metarun_flag = get_metarun_quest_flag(quest_idx);
    int metarun_count = metarun_flag ? metarun_quest_completion_count(metarun_flag) : 0;
    bool oath_override = false;
    if (q_ptr->oath_id > 0 && p_ptr && p_ptr->oath_type == q_ptr->oath_id && !oath_invalid(q_ptr->oath_id)) {
        oath_override = true;
    }

    if (metarun_flag) {
        if (metarun_count >= METARUN_QUEST_COMPLETION_CAP) {
            log_trace("Quest %d eligibility: METARUN_CAP (%d/%d) = FAIL", quest_idx, metarun_count, METARUN_QUEST_COMPLETION_CAP);
            return false;
        }
        if (metarun_count > 0 && !oath_override) {
            log_trace("Quest %d eligibility: METARUN_COMPLETED (count=%d) without oath override = FAIL", quest_idx, metarun_count);
            return false;
        }
        if (metarun_count > 0 && !metarun_repeat_tier_unlocked(metarun_count)) {
            int tier_count = metarun_quests_completed_at_least(metarun_count);
            log_trace("Quest %d eligibility: REPEAT_TIER_LOCKED (%d quest(s) at count >= %d, need %d) = FAIL",
                      quest_idx, tier_count, metarun_count, QUEST_REPEAT_TIER_REQUIRED);
            return false;
        }
        if (metarun_count > 0 && oath_override) {
            log_trace("Quest %d eligibility: metarun completion count=%d overridden by active oath %d and repeat tier",
                      quest_idx, metarun_count, q_ptr->oath_id);
        }
    }

    if (!quest_can_initiate_more()) {
        log_trace("Quest %d eligibility: initiated quest cap reached (%d/%d) = FAIL",
                  quest_idx, quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);
        return false;
    }

    /* 2. Check quest-specific state (must not be started for roulette quests) */
    switch (quest_idx) {
        case 1: /* Tulkas */
            if (p_ptr->tulkas_quest != TULKAS_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: TULKAS_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            if (!tulkas_has_valid_target(depth)) {
                log_trace("Quest %d eligibility: TULKAS_NO_TARGETS = FAIL (depth=%d)", quest_idx, depth);
                return false;
            }
            break;
        case 2: /* Aulë */
            if (p_ptr->aule_quest != AULE_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: AULE_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            break;
        case 3: /* Mandos */
            if (p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: MANDOS_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            break;
        case 4: /* Nienna */
            if (p_ptr->niena_quest != NIENA_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: NIENA_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            break;
        case 5: /* Oromë */
            if (p_ptr->orome_quest != OROME_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: OROME_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            break;
        case 6: /* Varda */
            if (p_ptr->varda_quest != VARDA_QUEST_NOT_STARTED) {
                log_trace("Quest %d eligibility: VARDA_ALREADY_STARTED = FAIL", quest_idx);
                return false;
            }
            break;
    }

    /* Check eligibility type from E: field */
    switch (q_ptr->eligibility_type) {
        case 0: /* No requirements */
            log_trace("Quest %d eligibility: NO_REQUIREMENTS = PASS", quest_idx);
            return true;

        case 1: /* SKILL_MIN - minimum skill requirement */
            if (q_ptr->eligibility_skill < S_MAX) {
                int player_skill = p_ptr->skill_base[q_ptr->eligibility_skill];
                bool meets_req = (player_skill >= q_ptr->eligibility_value);

                /* Enhanced logging for debugging smith skill issues */
                if (q_ptr->eligibility_skill == S_SMT) {
                    log_trace("SMT SKILL DEBUG: Quest %d eligibility check:", quest_idx);
                    log_trace("  skill_base[S_SMT] = %d", p_ptr->skill_base[S_SMT]);
                    log_trace("  skill_use[S_SMT] = %d", p_ptr->skill_use[S_SMT]);
                    log_trace("  required value = %d", q_ptr->eligibility_value);
                    log_trace("  meets requirement = %s", meets_req ? "YES" : "NO");
                }

                log_trace("Quest %d eligibility: SKILL_MIN (skill=%d, player=%d, required=%d) = %s",
                         quest_idx, q_ptr->eligibility_skill, player_skill, q_ptr->eligibility_value,
                         meets_req ? "PASS" : "FAIL");
                return meets_req;
            }
            return false;

        case 2: /* SKILL_RANGE - skill requirement within depth range */
            if (q_ptr->eligibility_skill < S_MAX &&
                depth >= q_ptr->eligibility_depth_min &&
                depth <= q_ptr->eligibility_depth_max) {
                int player_skill = p_ptr->skill_base[q_ptr->eligibility_skill];
                bool meets_req = (player_skill >= q_ptr->eligibility_value);
                log_trace("Quest %d eligibility: SKILL_RANGE (skill=%d, player=%d, required=%d, depth=%d in %d-%d) = %s",
                         quest_idx, q_ptr->eligibility_skill, player_skill, q_ptr->eligibility_value,
                         depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max,
                         meets_req ? "PASS" : "FAIL");
                return meets_req;
            }
            log_trace("Quest %d eligibility: SKILL_RANGE (depth=%d NOT in %d-%d) = FAIL",
                     quest_idx, depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max);
            return false;

        case 3: /* DEPTH_RANGE - must be within depth range */
            {
                bool in_range = (depth >= q_ptr->eligibility_depth_min && depth <= q_ptr->eligibility_depth_max);
                log_trace("Quest %d eligibility: DEPTH_RANGE (depth=%d in %d-%d) = %s",
                         quest_idx, depth, q_ptr->eligibility_depth_min, q_ptr->eligibility_depth_max,
                         in_range ? "PASS" : "FAIL");
                return in_range;
            }

        default:
            log_trace("Quest %d eligibility: Unknown type %d = FAIL", quest_idx, q_ptr->eligibility_type);
            return false;
    }
}

/*
 * Extract quest initialization texts from quest data
 * Returns array of text strings split by paragraph breaks
 */
cptr* extract_quest_init_texts(int quest_idx, int* count)
{
    quest_type* q_ptr;
    cptr full_text;
    cptr* texts;
    char* text_copy;
    char* line_start;
    char* line_end;
    int text_count = 0;
    int max_texts = 20; /* Maximum expected paragraphs */
    int len;

    /* Initialize count */
    if (count) *count = 0;
    else return NULL;

    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return NULL;

    q_ptr = &quest_info[quest_idx];
    if (!q_ptr->init_text) return NULL;

    /* Get the initialization text */
    full_text = q_text + q_ptr->init_text;
    if (!full_text || strlen(full_text) == 0) return NULL;

    /* Allocate text array */
    texts = mem_alloc_array(max_texts, cptr);
    if (!texts) return NULL;

    /* Create a working copy of the text */
    len = strlen(full_text);
    text_copy = mem_alloc_array(len + 1, char);
    if (!text_copy) {
        mem_free_null(texts);
        return NULL;
    }
    SDL_strlcpy(text_copy, full_text, len + 1);

    /* Split text by single newlines (each I: line becomes an entry) */
    line_start = text_copy;
    while (line_start && *line_start && text_count < max_texts - 1) {
        /* Find the end of this line */
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }

        /* Store the line (even if empty - empty lines become paragraph breaks) */
        texts[text_count] = str_dup(line_start);
        if (texts[text_count]) text_count++;

        /* Move to next line */
        if (line_end) {
            line_start = line_end + 1;
        } else {
            /* No more lines to process */
            break;
        }
    }

    /* Clean up */
    mem_free_null(text_copy);

    *count = text_count;
    return texts;
}

/*
 * Extract quest completion texts from quest data
 * Returns array of text strings split by paragraph breaks
 */
cptr* extract_quest_completion_texts(int quest_idx, int* count)
{
    quest_type* q_ptr;
    cptr full_text;
    cptr* texts;
    char* text_copy;
    char* line_start;
    char* line_end;
    int text_count = 0;
    int max_texts = 20; /* Maximum expected paragraphs */
    int len;

    /* Initialize count */
    if (count) *count = 0;
    else return NULL;

    /* Validate quest index */
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return NULL;

    q_ptr = &quest_info[quest_idx];
    if (!q_ptr->completion_text) return NULL;

    /* Get the completion text */
    full_text = q_text + q_ptr->completion_text;
    if (!full_text || strlen(full_text) == 0) return NULL;

    /* Allocate text array */
    texts = mem_alloc_array(max_texts, cptr);
    if (!texts) return NULL;

    /* Create a working copy of the text */
    len = strlen(full_text);
    text_copy = mem_alloc_array(len + 1, char);
    if (!text_copy) {
        mem_free_null(texts);
        return NULL;
    }
    SDL_strlcpy(text_copy, full_text, len + 1);

    /* Split text by single newlines (each W: line becomes an entry) */
    line_start = text_copy;
    while (line_start && text_count < max_texts - 1) {
        /* Find the end of this line */
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }

        /* Store the line (even if empty - empty lines become paragraph breaks) */
        texts[text_count] = str_dup(line_start);
        if (texts[text_count]) text_count++;

        /* Move to next line */
        if (line_end) {
            line_start = line_end + 1;
        } else {
            /* This was the last line, we're done */
            break;
        }
    }

    /* Clean up */
    mem_free_null(text_copy);

    *count = text_count;
    return texts;
}
