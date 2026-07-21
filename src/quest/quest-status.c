#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "quest/quest-internal.h"
#include "ui/story_font.h"

/* Forward declarations for quest text helpers used before their definitions */
static cptr get_quest_title(int quest_idx);
cptr get_oath_name_from_id(byte oath_id);

/* Prepend a repeat-attempt context line when returning to a Valar quest under an oath */
cptr* prepend_repeat_context(int quest_idx, cptr* texts, int* count, bool is_completion)
{
    if (!texts || !count || quest_idx <= 0 || quest_idx >= z_info->quest_max) return texts;

    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr || q_ptr->oath_id <= 0) return texts;
    if (!p_ptr || p_ptr->oath_type != q_ptr->oath_id || oath_invalid(q_ptr->oath_id)) return texts;

    u32b metarun_flag = get_metarun_quest_flag(quest_idx);
    int previous = metarun_flag ? metarun_quest_completion_count(metarun_flag) : 0;
    if (previous <= 0) return texts; /* First attempt - no alternate text */

    cptr quest_title = get_quest_title(quest_idx);
    cptr oath_name = get_oath_name_from_id(q_ptr->oath_id);

    char repeat_line[180];
    strnfmt(repeat_line, sizeof(repeat_line),
            is_completion
                ? "%s honors your %s oath after %d earlier success%s."
                : "%s returns under your %s oath; you have succeeded %d time%s before.",
            quest_title ? quest_title : "This quest",
            oath_name ? oath_name : "oath",
            previous,
            (previous == 1 ? "" : "s"));

    cptr* new_texts = mem_alloc_array(*count + 1, cptr);
    if (!new_texts) return texts;
    new_texts[0] = str_dup(repeat_line);
    for (int i = 0; i < *count; i++) new_texts[i + 1] = texts[i];
    mem_free_null(texts); /* free only the array container; strings move to new_texts */
    (*count)++;
    return new_texts;
}

/*
 * Get quest title from quest data
 */
static cptr get_quest_title(int quest_idx)
{
    log_trace("QUEST TITLE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);

    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST TITLE: Invalid bounds check, returning Unknown Quest");
        return "Unknown Quest";
    }

    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST TITLE: q_ptr is NULL, returning Unknown Quest");
        return "Unknown Quest";
    }

    if (q_ptr->title_text && q_text) {
        log_trace("QUEST TITLE: Using title_text");
        return q_text + q_ptr->title_text;
    }

    /* Fallback to quest name */
    if (q_ptr->name && quest_name_text) {
        log_trace("QUEST TITLE: Using quest name fallback");
        return quest_name_text + q_ptr->name;
    }

    log_trace("QUEST TITLE: No valid text found, returning Unknown Quest");
    return "Unknown Quest";
}

/*
 * Get quest challenge description from quest data
 */
static cptr get_quest_challenge(int quest_idx)
{
    log_trace("QUEST CHALLENGE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);

    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST CHALLENGE: Invalid bounds check, returning Unknown challenge");
        return "Unknown challenge";
    }

    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST CHALLENGE: q_ptr is NULL, returning Unknown challenge");
        return "Unknown challenge";
    }

    if (q_ptr->challenge_text && q_text) {
        log_trace("QUEST CHALLENGE: Using challenge_text");
        return q_text + q_ptr->challenge_text;
    }

    log_trace("QUEST CHALLENGE: No valid text found, returning default");
    return "Face the unknown challenge";
}

/*
 * Get oath name from oath ID using oath_info data
 */
cptr get_oath_name_from_id(byte oath_id)
{
    if (oath_id <= 0 || oath_id >= z_info->oath_max) return "No oath";

    oath_type* o_ptr = &oath_info[oath_id];
    if (o_ptr->name) {
        return oath_name_text + o_ptr->name;
    }

    /* Fallback to hardcoded names if oath_info not loaded */
    switch(oath_id) {
        case 0: return "No oath";
        case 1: return "Mercy oath";
        case 2: return "Silence oath";
        case 3: return "Iron oath";
        case 4: return "Smith oath";
        default: return "Unknown oath";
    }
}

/*
 * Display wrapped text for quest status - simple word wrapping
 */
void display_wrapped_text(int col, int *row, cptr text, byte color, int max_width)
{
    char line_buf[256];
    int line_pos = 0;
    int effective_width = max_width - col - 4; /* Leave margin for indentation */
    int text_len = strlen(text);
    int word_start = 0;
    int i = 0;
    int loop_count = 0; /* Safety counter for this function call */

    if (effective_width < 20) effective_width = 20; /* Minimum width */

    line_buf[0] = '\0';

    while (i <= text_len) {
        /* Safety check to prevent infinite loop */
        loop_count++;
        if (loop_count > 1000) {
            log_warn("display_wrapped_text: safety break, possible infinite loop (text_len=%d, i=%d)", text_len, i);
            break;
        }

        /* End of string or found a space */
        if (i == text_len || text[i] == ' ') {
            /* Extract the current word */
            int word_len = i - word_start;
            char word[128];

            if (word_len > 0 && word_len < (int)sizeof(word)) {
                /* Copy the word manually to avoid buffer issues */
                int copy_len = word_len;
                if (copy_len >= (int)sizeof(word)) copy_len = (int)sizeof(word) - 1;

                /* Manual copy to avoid strncpy issues */
                int j;
                for (j = 0; j < copy_len; j++) {
                    word[j] = text[word_start + j];
                }
                word[copy_len] = '\0';

                /* Check if adding this word would exceed the line width */
                int new_line_len = line_pos + (line_pos > 0 ? 1 : 0) + copy_len;

                if (new_line_len > effective_width && line_pos > 0) {
                    /* Output current line and start new line with this word */
                    Term_putstr(col + 2, (*row)++, -1, color, line_buf);

                    /* Check if the word itself is too long for a line */
                    if (copy_len > effective_width) {
                        /* Break the word across multiple lines */
                        int word_pos = 0;
                        while (word_pos < copy_len) {
                            int chunk_len = effective_width;
                            if (word_pos + chunk_len > copy_len) {
                                chunk_len = copy_len - word_pos;
                            }

                            /* Extract chunk of the word */
                            char chunk[256];
                            int k;
                            for (k = 0; k < chunk_len && word_pos + k < copy_len; k++) {
                                chunk[k] = word[word_pos + k];
                            }
                            chunk[k] = '\0';

                            /* Output this chunk */
                            Term_putstr(col + 2, (*row)++, -1, color, chunk);
                            word_pos += chunk_len;
                        }

                        /* Reset line buffer */
                        line_buf[0] = '\0';
                        line_pos = 0;
                    } else {
                        /* Word fits on a new line */
                        SDL_strlcpy(line_buf, word, sizeof(line_buf));
                        line_pos = copy_len;
                    }
                } else {
                    /* Add word to current line */
                    if (line_pos > 0) {
                        SDL_strlcat(line_buf, " ", sizeof(line_buf));
                        line_pos++;
                    }
                    SDL_strlcat(line_buf, word, sizeof(line_buf));
                    line_pos += copy_len;
                }
            }

            /* Skip spaces and move to next word */
            while (i < text_len && text[i] == ' ') {
                i++;
            }
            word_start = i;
        } else {
            i++;
        }
    }

    /* Output any remaining text in the buffer */
    if (line_pos > 0) {
        Term_putstr(col + 2, (*row)++, -1, color, line_buf);
    }
}

static bool quest_status_tabs_focus = false;
static int quest_status_content_col = 0;

static bool hint_quest_tab_key(char ch)
{
    return (ch == '\t');
}

static bool hint_quest_handle_tab_navigation(char ch, bool* tabs_focus,
    bool can_focus_tabs, hint_quest_page* next_page)
{
    int d = target_dir(ch);

    if (!tabs_focus || !next_page)
        return false;

    if (!*tabs_focus)
    {
        if (can_focus_tabs && d && !ddx[d] && (ddy[d] < 0))
        {
            *tabs_focus = true;
            return true;
        }

        return false;
    }

    if (d)
    {
        if (ddx[d])
        {
            *next_page = (ddx[d] < 0)
                ? HINT_QUEST_PAGE_HINTS : HINT_QUEST_PAGE_THRALLS;
            return true;
        }
        if (ddy[d] > 0)
        {
            *tabs_focus = false;
            return true;
        }
        if (ddy[d] < 0)
            return true;
    }

    return false;
}

static void quest_status_reset_page(int col, int *row)
{
    quest_status_content_col = col;
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
    sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_QUESTS,
        "Hints & Quests", "Quest Status", true, false, 0);
    if (row)
        *row = 0;
}

static void quest_status_put_line(int col, int hgt, int *row, byte color, cptr text)
{
    int indent = MAX(0, (col - quest_status_content_col) / 2);

    (void)hgt;
    sdl_hint_quest_menu_add_block(text ? text : "", color, indent, 0);
    if (row)
        (*row)++;
}

static void quest_status_put_wrapped(int col, int wid, int hgt, int *row,
    byte color, cptr text)
{
    int indent = MAX(0, (col - quest_status_content_col) / 2);

    (void)wid;
    (void)hgt;
    sdl_hint_quest_menu_add_block(text ? text : "", color, indent, 0);
    if (row)
        (*row)++;
}

/*
 * Simple string search function - finds needle in haystack
 * Returns pointer to first occurrence, or NULL if not found
 */
char* my_strstr(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;

    int needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;

    for (const char* p = haystack; *p; p++) {
        int i;
        for (i = 0; i < needle_len && p[i] && p[i] == needle[i]; i++);
        if (i == needle_len) {
            return (char*)p;
        }
    }
    return NULL;
}

/*
 * Process placeholders in quest text (challenge, etc.) with actual values
 */
static cptr process_quest_placeholders(cptr text, int quest_idx)
{
    static char processed_buf[256];

    if (!text) {
        return "";
    }

    SDL_strlcpy(processed_buf, text, sizeof(processed_buf));

    if (quest_idx == QUEST_ID_TULKAS) {
        /* Replace [monster name] with actual monster name */
        char* monster_pos = my_strstr(processed_buf, "[monster name]");
        if (monster_pos && p_ptr->tulkas_target_r_idx > 0 && p_ptr->tulkas_target_r_idx < z_info->r_max) {
            monster_race* r_ptr = &r_info[p_ptr->tulkas_target_r_idx];
            char before[128], after[128];
            int before_len = monster_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, r_name + r_ptr->name, after);
        }

        /* Replace [artifact name] with actual artifact name */
        char* artifact_pos = my_strstr(processed_buf, "[artifact name]");
        if (artifact_pos && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
            artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
            char before[128], after[128];
            int before_len = artifact_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, artifact_pos + 15, sizeof(after)); /* 15 = strlen("[artifact name]") */

            /* Get proper artifact name using object_desc */
            char artifact_name[120];
            if (a_ptr->name[0] != '\0') {
                /* Create a temporary object to get proper description */
                object_type temp_obj;
                object_wipe(&temp_obj);

                /* Set up the object as the artifact */
                s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
                if (k_idx > 0) {
                    object_prep(&temp_obj, k_idx);
                    temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                    temp_obj.ident |= IDENT_KNOWN;

                    /* Get the full artifact description */
                    object_desc(artifact_name, sizeof(artifact_name), &temp_obj, true, 0);
                } else {
                    SDL_strlcpy(artifact_name, a_ptr->name, sizeof(artifact_name));
                }
            } else {
                SDL_strlcpy(artifact_name, "a legendary weapon", sizeof(artifact_name));
            }

            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, artifact_name, after);
        }
    }

    return processed_buf;
}

/*
 * Get quest reward description for status display using actual quest data
 */
static cptr get_quest_reward_text(int quest_idx)
{
    static char reward_buf[200];
    char temp_buf[100];

    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return "Unknown reward";

    quest_type* q_ptr = &quest_info[quest_idx];
    reward_buf[0] = '\0';

    /* Handle special Tulkas artifact reward */
    if (quest_idx == QUEST_ID_TULKAS && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
        artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
        if (a_ptr->name[0] != '\0') {
            /* Create a temporary object to get proper description */
            object_type temp_obj;
            object_wipe(&temp_obj);

            /* Set up the object as the artifact */
            s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (k_idx > 0) {
                object_prep(&temp_obj, k_idx);
                temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                temp_obj.ident |= IDENT_KNOWN;

                /* Get the full artifact description */
                object_desc(reward_buf, sizeof(reward_buf), &temp_obj, true, 0);
                return reward_buf;
            } else {
                SDL_strlcpy(reward_buf, a_ptr->name, sizeof(reward_buf));
                return reward_buf;
            }
        }
    }

    /* Varda reward description */
    if (quest_idx == QUEST_ID_VARDA) {
        SDL_strlcpy(reward_buf, "Choose one radiant artefact and unlock the Oath of Light (+1 light radius)", sizeof(reward_buf));
        return reward_buf;
    }

    /* Build reward description from quest data */
    bool has_rewards = false;

    /* Check stat bonuses */
    if (q_ptr->stat_bonuses[0] || q_ptr->stat_bonuses[1] || q_ptr->stat_bonuses[2] || q_ptr->stat_bonuses[3]) {
        has_rewards = true;
        SDL_strlcat(reward_buf, "Stats: ", sizeof(reward_buf));

        if (q_ptr->stat_bonuses[0]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Str ", q_ptr->stat_bonuses[0]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[1]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Dex ", q_ptr->stat_bonuses[1]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[2]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Con ", q_ptr->stat_bonuses[2]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[3]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Gra ", q_ptr->stat_bonuses[3]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
    }

    /* Check skill bonuses */
    if (q_ptr->skill_type && q_ptr->skill_bonus) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;

        /* Convert skill type to name */
        cptr skill_name = "Unknown";
        switch (q_ptr->skill_type) {
            case 0: skill_name = "Melee"; break;
            case 1: skill_name = "Archery"; break;
            case 2: skill_name = "Evasion"; break;
            case 3: skill_name = "Stealth"; break;
            case 4: skill_name = "Perception"; break;
            case 5: skill_name = "Will"; break;
            case 6: skill_name = "Smithing"; break;
            case 7: skill_name = "Song"; break;
        }
        strnfmt(temp_buf, sizeof(temp_buf), "+%d %s ", q_ptr->skill_bonus, skill_name);
        SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
    }

    /* Check special abilities */
    if (q_ptr->ability_type && q_ptr->ability_id < ABILITIES_MAX) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;

        /* Get ability name from ability database */
        cptr ability_name = "Special ability";
        if (q_ptr->ability_type == S_SPC) { /* Special abilities type */
            /* Use ability_index to find the ability and get its name */
            int idx = ability_index(S_SPC, q_ptr->ability_id);
            if (idx >= 0 && idx < z_info->b_max) {
                ability_type* b_ptr = &b_info[idx];
                if (b_ptr->name) {
                    ability_name = b_name + b_ptr->name;
                }
            }
        }

        SDL_strlcat(reward_buf, ability_name, sizeof(reward_buf));
    }

    /* Check oath association */
    if (q_ptr->oath_id) {
        if (has_rewards) SDL_strlcat(reward_buf, " | ", sizeof(reward_buf));
        has_rewards = true;
        SDL_strlcat(reward_buf, get_oath_name_from_id(q_ptr->oath_id), sizeof(reward_buf));
    }

    if (!has_rewards) {
        SDL_strlcpy(reward_buf, "Unknown reward", sizeof(reward_buf));
    }

    return reward_buf;
}

/*
 * Free quest text array returned by extract functions
 */
void free_quest_texts(cptr* texts, int count)
{
    if (!texts) return;

    if (count < 0) count = 0;
    if (count > 50) count = 50; /* hard cap safety */

    for (int i = 0; i < count; i++) {
        if (texts[i]) {
            str_free((char*)texts[i]);
        }
    }

    mem_free_null(texts);
}

/*
 * Show quest status for current metarun - only active and completed quests
 * Now uses quest.txt data instead of hardcoded values
 */
hint_quest_page do_cmd_quest_status_page(void)
{
    char buf[128];
    int row = 0;
    int col = 0;
    bool any_quests = false;
    int wid = 0;
    int hgt = 0;
    hint_quest_page next_page = HINT_QUEST_PAGE_EXIT;

    log_trace("QUEST STATUS: SDL book page opened");

    /* Safety check: ensure we have a valid player and metarun */
    if (!p_ptr) {
        log_trace("QUEST STATUS: No player data available");
        msg_print("No character data available.");
        return HINT_QUEST_PAGE_EXIT;
    }

    log_trace("QUEST STATUS: Player exists, quest states - Tulkas: %d, Aulë: %d, Mandos: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest);

    quest_status_tabs_focus = true;
    quest_status_reset_page(col, &row);

    /* Check Tulkas quest */
    if (p_ptr->tulkas_quest > TULKAS_QUEST_GIVER_PRESENT) {
        any_quests = true;
        cptr tulkas_status;
        byte color;

        log_trace("QUEST STATUS: Getting title and challenge for Tulkas quest");
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_TULKAS);
        log_trace("QUEST STATUS: Got title='%s', challenge='%s'", quest_title ? quest_title : "NULL", quest_challenge ? quest_challenge : "NULL");

        if (!quest_title) quest_title = "Tulkas Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";

        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);

        switch (p_ptr->tulkas_quest) {
            case TULKAS_QUEST_GIVER_PRESENT:
                log_trace("QUEST STATUS: Tulkas GIVER_PRESENT case");
                tulkas_status = "Available - Tulkas awaits";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
                {
                    log_trace("QUEST STATUS: About to call process_quest_placeholders");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, processed_challenge);
                }
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (GIVER_PRESENT)");
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case TULKAS_QUEST_ACTIVE:
                log_trace("QUEST STATUS: Tulkas ACTIVE case");
                {
                    /* Use processed challenge text instead of hardcoded status */
                    log_trace("QUEST STATUS: About to call process_quest_placeholders for ACTIVE");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_WHITE, processed_challenge);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                    break;
                }
            case TULKAS_QUEST_COMPLETE:
                log_trace("QUEST STATUS: Tulkas COMPLETE case");
                tulkas_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
                break;
            case TULKAS_QUEST_REWARDED:
                /* For Tulkas quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                tulkas_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (REWARDED)");
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                tulkas_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, tulkas_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Aulë quest */
    if (p_ptr->aule_quest > AULE_QUEST_FORGE_PRESENT) {
        any_quests = true;
        cptr aule_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_AULE);

        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);

        switch (p_ptr->aule_quest) {
            case AULE_QUEST_FORGE_PRESENT:
                aule_status = "Available - Enter the forge";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case AULE_QUEST_ACTIVE:
                /* Use challenge text instead of hardcoded status */
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_WHITE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case AULE_QUEST_SUCCESS:
                aule_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case AULE_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                aule_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_AULE));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                aule_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, aule_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Mandos quest */
    if (p_ptr->mandos_quest > MANDOS_QUEST_GIVER_PRESENT) {
        any_quests = true;
        cptr mandos_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_MANDOS);

        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);

        switch (p_ptr->mandos_quest) {
            case MANDOS_QUEST_GIVER_PRESENT:
                mandos_status = "Available - Enter the tomb";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_ACTIVE:
                mandos_status = "Active";
                color = TERM_WHITE;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_SUCCESS:
                mandos_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                mandos_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_MANDOS));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                mandos_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, mandos_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Nienna quest */
    if (p_ptr->niena_quest > NIENA_QUEST_GIVER_PRESENT) {
        any_quests = true;
        cptr niena_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_NIENA);

        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);

        switch (p_ptr->niena_quest) {
            case NIENA_QUEST_GIVER_PRESENT:
                niena_status = "Available - Nienna offers mercy";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_ACTIVE:
                strnfmt(buf, sizeof(buf), "Active: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                niena_status = buf;
                color = TERM_WHITE;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_SUCCESS:
                niena_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                niena_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_NIENA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_FAILED:
                strnfmt(buf, sizeof(buf), "Failed: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                color = TERM_RED;
                quest_status_put_line(col + 2, hgt, &row, color, buf);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE,
                    "You took a life and lost Nienna's mercy.");
                break;
            default:
                niena_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, niena_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Oromë quest */
    if (p_ptr->orome_quest > OROME_QUEST_GIVER_PRESENT) {
        any_quests = true;
        cptr orome_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_OROME);

        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);

        switch (p_ptr->orome_quest) {
            case OROME_QUEST_GIVER_PRESENT:
                orome_status = "Available - Oromë awaits";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case OROME_QUEST_ACTIVE:
                {
                    strnfmt(buf, sizeof(buf), "Active: Hunt the fell kindreds");
                    orome_status = buf;
                    color = TERM_WHITE;
                    quest_status_put_line(col + 2, hgt, &row, color, orome_status);

                    /* Show current kill counts for all monster types */
                    strnfmt(buf, sizeof(buf), "Wolves killed: %d/100", p_ptr->orome_wolves_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_wolves_killed >= 100 ? TERM_L_GREEN : TERM_SLATE, buf);
                    strnfmt(buf, sizeof(buf), "Spiders killed: %d/80", p_ptr->orome_spiders_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_spiders_killed >= 80 ? TERM_L_GREEN : TERM_SLATE, buf);
                    strnfmt(buf, sizeof(buf), "Serpents killed: %d/60", p_ptr->orome_serpents_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_serpents_killed >= 60 ? TERM_L_GREEN : TERM_SLATE, buf);
                    strnfmt(buf, sizeof(buf), "Vampires killed: %d/30", p_ptr->orome_vampires_killed);
                    quest_status_put_wrapped(col + 4, wid, hgt, &row,
                        p_ptr->orome_vampires_killed >= 30 ? TERM_L_GREEN : TERM_SLATE, buf);

                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                }
                break;
            case OROME_QUEST_SUCCESS:
                orome_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case OROME_QUEST_REWARDED:
                /* For Oromë quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                orome_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_OROME));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            default:
                orome_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, orome_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Check Varda quest */
    if (p_ptr->varda_quest > VARDA_QUEST_GIVER_PRESENT) {
        any_quests = true;
        cptr varda_status;
        byte color;
        bool on_bastion_level = varda_quest_bastion_level_active();

        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_VARDA);
        if (!quest_title) quest_title = "Varda Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";

        quest_status_put_line(col, hgt, &row, TERM_YELLOW, quest_title);

        switch (p_ptr->varda_quest) {
            case VARDA_QUEST_GIVER_PRESENT:
                varda_status = "Available - Varda waits in sunlight";
                color = TERM_L_BLUE;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_ACTIVE:
                if (on_bastion_level) {
                    varda_status = "Active - Duruin's Bastion is on this level";
                    color = TERM_ORANGE;
                } else {
                    varda_status = "Active - Seek Duruin's Bastion";
                    color = TERM_WHITE;
                }
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, quest_challenge);
                if (on_bastion_level) {
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_ORANGE,
                        "This is the first level you have reached after 500 ft. Slay Duruin before leaving, or Varda's quest is lost.");
                } else {
                    quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE,
                        "Duruin's Bastion will appear on the first level you reach after 500 ft.");
                }
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_SUCCESS:
                varda_status = "Complete - Claim Varda's blessing";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_REWARDED:
                varda_status = "Completed by this character";
                color = TERM_L_GREEN;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_VARDA));
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, buf);
                break;
            case VARDA_QUEST_FAILED:
                varda_status = "Failed - Duruin's Bastion was left behind";
                color = TERM_RED;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
                quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE,
                    "Varda's trial was on the first level you reached after 500 ft; leaving it without slaying Duruin ended the quest.");
                break;
            default:
                varda_status = "Unknown status";
                color = TERM_SLATE;
                quest_status_put_line(col + 2, hgt, &row, color, varda_status);
        }
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* Show previous tale completions */
    bool has_previous_completions = false;
    int tulkas_completed = metarun_quest_completion_count(METARUN_QUEST_TULKAS);
    if (tulkas_completed > 0 && p_ptr->tulkas_quest != TULKAS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in This Tale:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr oath_name = get_oath_name_from_id(quest_info[1].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (x%d)", quest_title, oath_name, tulkas_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int aule_completed = metarun_quest_completion_count(METARUN_QUEST_AULE);
    if (aule_completed > 0 && p_ptr->aule_quest != AULE_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in This Tale:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr oath_name = get_oath_name_from_id(quest_info[2].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (x%d)", quest_title, oath_name, aule_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int mandos_completed = metarun_quest_completion_count(METARUN_QUEST_MANDOS);
    if (mandos_completed > 0 && p_ptr->mandos_quest != MANDOS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in This Tale:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr oath_name = get_oath_name_from_id(quest_info[3].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (x%d)", quest_title, oath_name, mandos_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int niena_completed = metarun_quest_completion_count(METARUN_QUEST_NIENA);
    if (niena_completed > 0 && p_ptr->niena_quest != NIENA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in This Tale:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr oath_name = get_oath_name_from_id(quest_info[4].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (x%d)", quest_title, oath_name, niena_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int orome_completed = metarun_quest_completion_count(METARUN_QUEST_OROME);
    if (orome_completed > 0 && p_ptr->orome_quest != OROME_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in This Tale:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr oath_name = get_oath_name_from_id(quest_info[5].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (x%d)", quest_title, oath_name, orome_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }
    int varda_completed = metarun_quest_completion_count(METARUN_QUEST_VARDA);
    if (varda_completed > 0 && p_ptr->varda_quest != VARDA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            quest_status_put_line(col, hgt, &row, TERM_L_DARK,
                "Previously Completed in This Tale:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr oath_name = get_oath_name_from_id(quest_info[6].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (x%d)", quest_title, oath_name, varda_completed);
        quest_status_put_wrapped(col, wid, hgt, &row, TERM_SLATE, status_text);
    }

    if (has_previous_completions) {
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    }

    /* If no quests are active or completed */
    if (!any_quests) {
        quest_status_put_line(col, hgt, &row, TERM_SLATE,
            "No active or completed quests this run.");
        quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
        quest_status_put_line(col, hgt, &row, TERM_L_DARK,
            "Quest vaults may appear as you delve deeper...");
    }

    quest_status_put_line(col, hgt, &row, TERM_WHITE, "");
    sdl_hint_quest_menu_add_button(HINT_QUEST_CLICK_RETURN, "Back",
        TERM_L_WHITE);
    sdl_hint_quest_menu_finish();
    Term_fresh();
    while (true) {
        char ch;
        bool saved_hide_cursor = hide_cursor;
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        hide_cursor = true;
        ch = inkey();
        hide_cursor = saved_hide_cursor;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (clicked_choice == HINT_QUEST_CLICK_RETURN)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                break;
            }
            if (clicked_choice == HINT_QUEST_CLICK_HINTS_TAB)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                {
                    quest_status_tabs_focus = false;
                    continue;
                }
                next_page = HINT_QUEST_PAGE_HINTS;
                break;
            }
            if (clicked_choice == HINT_QUEST_CLICK_QUESTS_TAB)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                {
                    quest_status_tabs_focus = false;
                }
                continue;
            }
            if (clicked_choice == HINT_QUEST_CLICK_THRALLS_TAB)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                {
                    quest_status_tabs_focus = false;
                    continue;
                }
                next_page = HINT_QUEST_PAGE_THRALLS;
                break;
            }
            if (click_action == UI_MENU_CLICK_HOVER)
            {
                quest_status_tabs_focus = false;
                continue;
            }
            break;
        }
        if (ch == UI_MENU_CLICK_WAKE_KEY)
            continue;
        if (hint_quest_tab_key(ch))
        {
            next_page = HINT_QUEST_PAGE_THRALLS;
            break;
        }
        if (hint_quest_handle_tab_navigation(ch,
                &quest_status_tabs_focus, true, &next_page))
        {
            if (next_page != HINT_QUEST_PAGE_EXIT)
                break;
            continue;
        }
        break;
    }

    ui_menu_click_clear();
    if (next_page != HINT_QUEST_PAGE_EXIT)
        sdl_hint_quest_menu_prepare_page_turn(next_page);
    else
        sdl_hint_quest_menu_hide();
    quest_status_tabs_focus = false;
    return next_page;
}
