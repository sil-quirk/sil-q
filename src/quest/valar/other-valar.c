#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "quest/quest-internal.h"

/*
 * Count hostile monsters in Mandos vault area using proper vault boundaries
 */
/*
 * Check if Brodda (formerly Aldor) has been killed for Mandos quest
 */
static bool is_brodda_dead(void)
{
    int i;

    /* Check if Brodda is still alive on the level */
    for (i = 1; i < mon_max; i++)
    {
        monster_type *m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_ALDOR) /* Brodda uses the same monster index as Aldor */
        {
            log_trace("Brodda is still alive at (%d, %d)", m_ptr->fy, m_ptr->fx);
            return false;
        }
    }

    log_trace("Brodda has been slain");
    return true;
}

/*
 * Check if player is adjacent to Aulë
 */
void check_aule_quest_interaction(void)
{
    int i, y, x;

    /* Only check if quest is in appropriate state */
    if (p_ptr->aule_quest != AULE_QUEST_NOT_STARTED &&
        p_ptr->aule_quest != AULE_QUEST_FORGE_PRESENT &&
        p_ptr->aule_quest != AULE_QUEST_SUCCESS)
    {
        return;
    }

    /* Skip interaction if quest already rewarded */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED)
    {
        return;
    }

    log_trace("check_aule_quest_interaction: checking adjacency, quest state: %d", p_ptr->aule_quest);

    /* Check all adjacent squares for Aulë */
    for (i = 1; i < 9; i++)
    {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];

        /* Check bounds */
        if (!in_bounds(y, x)) continue;

        /* Check for monster */
        if (cave_m_idx[y][x] > 0)
        {
            int m_idx = cave_m_idx[y][x];

            if (m_idx >= mon_max) continue;

            monster_type* m_ptr = &mon_list[m_idx];

            /* Check if it's Aulë */
            if (m_ptr->r_idx == R_IDX_AULE)
            {
                log_trace("Found Aulë adjacent, calling interaction");
                aule_quest_interaction();
                return;
            }
        }
    }
}

/*
 * Handle Aulë interaction
 */
void aule_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    /* Skip interaction if quest already rewarded */
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED)
    {
        log_trace("Quest already rewarded, no interaction");
        return;
    }

    /* Handle first encounter - initialize quest */
    if (p_ptr->aule_quest == AULE_QUEST_NOT_STARTED)
    {
        if (!quest_can_initiate_more()) {
            msg_print("You cannot take on another quest in this life.");
            log_trace("Aulë quest: initiate blocked by cap (%d/%d)",
                      quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);
            return;
        }

        log_trace("First encounter with Aulë - setting to FORGE_PRESENT");
        p_ptr->aule_quest = AULE_QUEST_FORGE_PRESENT;
        p_ptr->aule_level = p_ptr->depth;
        quest_note_initiated(QUEST_ID_AULE);
        /* Don't start the actual quest conversation yet, let them talk again */
        msg_print("You encounter Aulë the Smith, Maker of Mountains.");
        msg_print("'Speak with me again to learn of the challenges that await.'");
        return;
    }

    /* Handle quest explanation */
    if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT)
    {
        if (!quest_can_accept_more()) {
            msg_print("You are already committed to two quests. Finish one before accepting another.");
            log_trace("Aulë quest: accept blocked by active quest cap (%d/%d)",
                      quest_accepted_count_this_run(), QUEST_MAX_ACCEPTED_PER_RUN);
            return;
        }

        log_trace("Aulë quest explanation - setting to ACTIVE");
        p_ptr->aule_quest = AULE_QUEST_ACTIVE;

        /* Only remove quest giver for roulette-based quests (Y:1) */
        quest_type* q_ptr = &quest_info[2]; /* Aulë is quest index 2 */
        if (q_ptr->quest_type == 1) { /* Y:1 = roulette-based */
            remove_quest_giver_silent(R_IDX_AULE);
            log_trace("Aulë quest giver removed silently (roulette-based quest)");
        } else {
            log_trace("Aulë quest giver NOT removed (vault-based quest)");
        }

        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(2, &text_count); /* Aulë is quest index 2 */
        init_texts = prepend_repeat_context(QUEST_ID_AULE, init_texts, &text_count, false);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Aulë the Smith", init_texts, text_count, TERM_YELLOW, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Aulë speaks in a voice like hammer on anvil:",
                "'Find my forge and create something worthy of my attention.'"
            };
            quest_typewriter_menu("Aulë the Smith", fallback_texts, 2, TERM_YELLOW, TERM_WHITE);
        }

        /* Mark in the notes */
        do_cmd_note("Aulë has challenged me to use his forge to create an item.", p_ptr->depth);
        return;
    }

    /* Handle quest completion */
    if (p_ptr->aule_quest == AULE_QUEST_SUCCESS)
    {
        log_trace("Aulë quest completed - giving special ability reward");

        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(2, &completion_count); /* Aulë is quest index 2 */
        completion_texts = prepend_repeat_context(QUEST_ID_AULE, completion_texts, &completion_count, true);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Quest Complete!", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Aulë nods with satisfaction:",
                "'Well done! Your skill at the forge shows promise.'"
            };
            quest_typewriter_menu("Quest Complete!", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }

        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_AULE);

        /* Change quest state to prevent repeated interactions */
        p_ptr->aule_quest = AULE_QUEST_REWARDED;

        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(2); /* Aulë is quest index 2 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }

        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(2); /* Aulë is quest index 2 */

        msg_print("Aulë smiles with approval and returns to his eternal labors.");

        /* Remove the quest giver after giving reward */
        remove_quest_giver_silent(R_IDX_AULE);

        return;
    }

    /* Handle other quest states */
    if (p_ptr->aule_quest == AULE_QUEST_ACTIVE)
    {
        msg_print("Aulë watches you with eyes like glowing coals:");
        msg_print("'The forge awaits your skill. Show me what you can create.'");
        return;
    }

    /* Default message */
    msg_print("Aulë the Smith regards you with interest.");
}

/*
 * Handle Mandos interaction
 */
void mandos_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    /* Handle first encounter - initialize quest */
    if (p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED)
    {
        if (!quest_can_initiate_more()) {
            msg_print("You cannot take on another quest in this life.");
            log_trace("Mandos quest: initiate blocked by cap (%d/%d)",
                      quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);
            return;
        }

        log_trace("First encounter with Mandos - setting to GIVER_PRESENT");
        p_ptr->mandos_quest = MANDOS_QUEST_GIVER_PRESENT;
        p_ptr->mandos_level = p_ptr->depth;
        quest_note_initiated(QUEST_ID_MANDOS);
        /* Don't start the actual quest conversation yet, let them talk again */
        msg_print("You encounter Mandos, the Doomsman of the Valar.");
        msg_print("His stern gaze weighs upon your soul, as if judging your worth.");
        return;
    }

    /* Safety check - ensure valid quest state */
    if (p_ptr->mandos_quest != MANDOS_QUEST_GIVER_PRESENT &&
        p_ptr->mandos_quest != MANDOS_QUEST_ACTIVE &&
        p_ptr->mandos_quest != MANDOS_QUEST_SUCCESS &&
        p_ptr->mandos_quest != MANDOS_QUEST_REWARDED)
    {
        log_trace("mandos_quest_interaction called with invalid quest state: %d", p_ptr->mandos_quest);
        return;
    }

    if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT)
    {
        if (!quest_can_accept_more()) {
            msg_print("You are already committed to two quests. Finish one before accepting another.");
            log_trace("Mandos quest: accept blocked by active quest cap (%d/%d)",
                      quest_accepted_count_this_run(), QUEST_MAX_ACCEPTED_PER_RUN);
            return;
        }

        log_trace("Starting Mandos quest interaction - assigning Brodda quest");

        /* Set quest state */
        p_ptr->mandos_quest = MANDOS_QUEST_ACTIVE;
        p_ptr->mandos_level = p_ptr->depth;

        /* Only remove quest giver for roulette-based quests (Y:1) */
        quest_type* q_ptr = &quest_info[3]; /* Mandos is quest index 3 */
        if (q_ptr->quest_type == 1) { /* Y:1 = roulette-based */
            remove_quest_giver_silent(R_IDX_MANDOS);
            log_trace("Mandos quest giver removed silently (roulette-based quest)");
        } else {
            log_trace("Mandos quest giver NOT removed (vault-based quest)");
        }

        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(3, &text_count); /* Mandos is quest index 3 */
        init_texts = prepend_repeat_context(QUEST_ID_MANDOS, init_texts, &text_count, false);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Mandos the Doomsman", init_texts, text_count, TERM_L_DARK, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Mandos speaks with the authority of the Valar:",
                "'Slay Brodda the Easterling and prove your worth.'"
            };
            quest_typewriter_menu("Mandos the Doomsman", fallback_texts, 2, TERM_L_DARK, TERM_WHITE);
        }

        log_trace("Mandos quest activated - player must slay Brodda");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_ACTIVE)
    {
        /* Check if Brodda is dead */
        if (is_brodda_dead())
        {
            p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;

            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(3, &completion_count); /* Mandos is quest index 3 */
            completion_texts = prepend_repeat_context(QUEST_ID_MANDOS, completion_texts, &completion_count, true);

            if (completion_texts && completion_count > 0) {
                quest_typewriter_menu("Justice Served", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Mandos nods with solemn approval:",
                    "'Justice has been served. The path forward opens.'"
                };
                quest_typewriter_menu("Justice Served", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
            }

            log_trace("Mandos quest completed successfully");
        }
        else
        {
            msg_print("Mandos gazes at you with penetrating eyes:");
            msg_print("'Brodda the Easterling still draws breath within these halls.");
            msg_print("Until his tyranny is ended, you may not pass beyond.'");
            msg_print("");
            msg_print("'Remember - he who ruled Dor-lomin with an iron fist");
            msg_print("must face the justice he denied to others.'");
        }
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_SUCCESS)
    {
        log_trace("Mandos quest already completed - giving special ability reward");

        /* We'll show the same completion texts again since this is the reward phase */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(3, &completion_count); /* Mandos is quest index 3 */
        completion_texts = prepend_repeat_context(QUEST_ID_MANDOS, completion_texts, &completion_count, true);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Quest Reward", completion_texts, completion_count, TERM_L_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Mandos acknowledges you with respect:",
                "'Accept the gift of my protection from mortal fears.'"
            };
            quest_typewriter_menu("Quest Reward", fallback_texts, 2, TERM_L_GREEN, TERM_WHITE);
        }

        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
        log_trace("Mandos quest: marked as completed in metarun");

        /* Change quest state to prevent repeated interactions */
        p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;

        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(3); /* Mandos is quest index 3 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }

        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(3); /* Mandos is quest index 3 */

        msg_print("Mandos bows deeply and fades into shadow, his task complete.");

        /* Remove the quest giver after giving reward */
        remove_quest_giver_silent(R_IDX_MANDOS);

        log_trace("Mandos quest reward given");
    }
    else if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED)
    {
        log_trace("Mandos quest already rewarded - giving acknowledgment");
        msg_print("Mandos nods with solemn respect:");
        msg_print("'The task is done, and the doom has been fulfilled.'");
        msg_print("'Your path continues ever deeper into the halls of Mandos.'");
    }
}

/*
 * Check if player is adjacent to Mandos and handle interaction
 */
void check_mandos_quest_interaction(void)
{
    int i, y, x;
    static s32b last_interaction_turn = -1;

    log_trace("check_mandos_quest_interaction called, quest state: %d, turn: %d", p_ptr->mandos_quest, turn);

    /* Prevent multiple interactions in the same turn */
    if (last_interaction_turn == turn)
    {
        log_trace("Already interacted this turn, skipping");
        return;
    }

    /* Only check if quest is in appropriate state */
    if (p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED &&
        p_ptr->mandos_quest != MANDOS_QUEST_GIVER_PRESENT &&
        p_ptr->mandos_quest != MANDOS_QUEST_ACTIVE &&
        p_ptr->mandos_quest != MANDOS_QUEST_SUCCESS &&
        p_ptr->mandos_quest != MANDOS_QUEST_REWARDED)
    {
        log_trace("Quest not in correct state (%d), returning", p_ptr->mandos_quest);
        return;
    }

    /* Check all adjacent squares for Mandos */
    for (i = 1; i < 9; i++)
    {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];

        if (in_bounds(y, x))
        {
            s16b m_idx = cave_m_idx[y][x];

            if (m_idx <= 0 || m_idx >= mon_max)
                continue;

            monster_type* m_ptr = &mon_list[m_idx];
            if (!m_ptr)
                continue;

            if (m_ptr->r_idx <= 0 || m_ptr->r_idx >= z_info->r_max)
                continue;

            if (m_ptr->r_idx == R_IDX_MANDOS)
            {
                log_trace("Found Mandos, calling interaction (turn %d)", turn);
                last_interaction_turn = turn;
                mandos_quest_interaction();
                return;
            }
        }
    }

    log_trace("No Mandos found adjacent");
}

/*
 * Handle monster death for Mandos quest
 */
void check_mandos_quest_completion(int r_idx)
{
    if (p_ptr->mandos_quest == MANDOS_QUEST_ACTIVE)
    {
        log_trace("Mandos quest: Checking completion after death of r_idx %d", r_idx);

        /* Check if Brodda was killed */
        if (r_idx == R_IDX_ALDOR)  /* Brodda (formerly Aldor) */
        {
            p_ptr->mandos_quest = MANDOS_QUEST_SUCCESS;

            msg_print("Brodda the Easterling falls! His tyranny is ended at last.");
            msg_print("The spirits of Dor-lomin can finally know peace.");
            msg_print("Return to Mandos the Doomsman to claim your reward.");

            log_trace("Mandos quest completed - Brodda slain");
        }
    }
}

/*
 * Handle quest completion checking for Oromë hunting quest
 */
void check_orome_quest_completion(void)
{
    if (p_ptr->orome_quest == OROME_QUEST_ACTIVE) {
        /* Check thresholds for each monster type */
        bool quest_complete = false;
        cptr monster_name = "";
        int kill_count = 0;

        if (p_ptr->orome_wolves_killed >= 100) {
            quest_complete = true;
            monster_name = "wolves";
            kill_count = p_ptr->orome_wolves_killed;
        }
        else if (p_ptr->orome_spiders_killed >= 80) {
            quest_complete = true;
            monster_name = "spiders";
            kill_count = p_ptr->orome_spiders_killed;
        }
        else if (p_ptr->orome_serpents_killed >= 60) {
            quest_complete = true;
            monster_name = "serpents";
            kill_count = p_ptr->orome_serpents_killed;
        }
        else if (p_ptr->orome_vampires_killed >= 30) {
            quest_complete = true;
            monster_name = "vampires";
            kill_count = p_ptr->orome_vampires_killed;
        }

        if (quest_complete) {
            p_ptr->orome_quest = OROME_QUEST_SUCCESS;

            msg_format("The hunt is complete! You have slain %d %s, proving your prowess!",
                       kill_count, monster_name);
            msg_print("Oromë the Huntsman will be pleased with your mastery.");
            msg_print("Seek him out to claim your reward - the knowledge of Unique Bane!");

            log_trace("Oromë quest completed - %d %s slain (wolves=%d, spiders=%d, serpents=%d, vampires=%d)",
                     kill_count, monster_name,
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed,
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
    }
}

/*
 * Handle interaction with Nienna for the mercy quest
 */
void niena_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    /* Safety check - ensure valid quest state */
    if (p_ptr->niena_quest != NIENA_QUEST_GIVER_PRESENT &&
        p_ptr->niena_quest != NIENA_QUEST_SUCCESS)
    {
        log_trace("niena_quest_interaction called with invalid quest state: %d", p_ptr->niena_quest);
        return;
    }

    if (p_ptr->niena_quest == NIENA_QUEST_GIVER_PRESENT)
    {
        if (!quest_can_accept_more()) {
            msg_print("You are already committed to two quests. Finish one before accepting another.");
            log_trace("Nienna quest: accept blocked by active quest cap (%d/%d)",
                      quest_accepted_count_this_run(), QUEST_MAX_ACCEPTED_PER_RUN);
            return;
        }

        log_trace("Starting Nienna quest interaction - offering mercy quest");

        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(4, &text_count); /* Nienna is quest index 4 */
        init_texts = prepend_repeat_context(QUEST_ID_NIENA, init_texts, &text_count, false);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Nienna, Lady of Pity", init_texts, text_count, TERM_L_BLUE, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Nienna, Lady of Pity, speaks with a voice full of sorrow and hope:",
                "'Show mercy to the creatures here and find the downward path.'"
            };
            quest_typewriter_menu("Nienna, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
        }

        /* Accept the quest */
        p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
        p_ptr->niena_monsters_seen = 0;
        p_ptr->niena_monsters_killed = 0;
        p_ptr->niena_level = p_ptr->depth; /* Track where quest was started */

        /* Remove the quest giver now that quest is accepted without
         * showing the generic reward/departure message before the quest text.
         */
        remove_quest_giver_silent(R_IDX_NIENA);

        /* Make all stairs visible */
        int y, x;
        for (y = 0; y < p_ptr->cur_map_hgt; y++) {
            for (x = 0; x < p_ptr->cur_map_wid; x++) {
                if (cave_feat[y][x] == FEAT_MORE || cave_feat[y][x] == FEAT_MORE_SHAFT ||
                    cave_feat[y][x] == FEAT_LESS || cave_feat[y][x] == FEAT_LESS_SHAFT) {
                    cave_info[y][x] |= CAVE_MARK;
                    cave_info[y][x] |= CAVE_SEEN;
                }
            }
        }

        msg_print("The stairs throughout the level become clearly visible to you.");
        msg_print("Nienna fades away, but her presence lingers in your heart.");

        /* Update display */
        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
        p_ptr->redraw |= (PR_MAP);
        handle_stuff();

        log_trace("Nienna quest started - all stairs revealed, monsters_seen=%d, monsters_killed=%d",
                 p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
    }
    else if (p_ptr->niena_quest == NIENA_QUEST_SUCCESS)
    {
        log_trace("Completing Nienna quest - giving enhanced stealth reward");

        /* Calculate the stealth bonus: 10 * (seen - killed) / seen, rounded up */
        int stealth_bonus = 0;
        if (p_ptr->niena_monsters_seen > 0) {
            /* Using ceiling division: (a + b - 1) / b */
            int mercy_ratio_times_10 = (10 * (p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed));
            stealth_bonus = (mercy_ratio_times_10 + p_ptr->niena_monsters_seen - 1) / p_ptr->niena_monsters_seen;
        }

        if (stealth_bonus > 0) {
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(4, &completion_count); /* Nienna is quest index 4 */
            completion_texts = prepend_repeat_context(QUEST_ID_NIENA, completion_texts, &completion_count, true);

            if (completion_texts && completion_count > 0) {
                quest_typewriter_menu("Nienna, Lady of Pity", completion_texts, completion_count, TERM_L_BLUE, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Nienna appears with tears of joy in her eyes!",
                    "'You have shown that true strength lies in restraint.'"
                };
                quest_typewriter_menu("Nienna, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
            }

            /* Show the specific numbers and bonus after the main dialogue */
            msg_format("You encountered %d creatures but spared %d of them.",
                      p_ptr->niena_monsters_seen, p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed);
            msg_format("You gain +%d effective stealth from your mercy.", stealth_bonus);
        } else {
            /* Extract completion texts from quest data */
            int completion_count = 0;
            cptr* completion_texts = extract_quest_completion_texts(4, &completion_count); /* Nienna is quest index 4 */
            completion_texts = prepend_repeat_context(QUEST_ID_NIENA, completion_texts, &completion_count, true);

            if (completion_texts && completion_count > 1) {
                /* Use alternate completion text if available */
                cptr alt_texts[] = {completion_texts[1]};
                quest_typewriter_menu("Nienna, Lady of Pity", alt_texts, 1, TERM_L_BLUE, TERM_WHITE);
                free_quest_texts(completion_texts, completion_count);
            } else {
                /* Fallback to simple message if text extraction fails */
                cptr fallback_texts[] = {
                    "Nienna appears, her expression neutral.",
                    "'You have completed the task, though perhaps not as I hoped.'"
                };
                quest_typewriter_menu("Nienna, Lady of Pity", fallback_texts, 2, TERM_L_BLUE, TERM_WHITE);
            }

            /* Show the specific numbers after the main dialogue */
            msg_format("You encountered %d creatures but spared %d of them.",
                      p_ptr->niena_monsters_seen, p_ptr->niena_monsters_seen - p_ptr->niena_monsters_killed);
        }

        /* Clear quest state */
        p_ptr->niena_quest = NIENA_QUEST_REWARDED;

        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(4); /* Nienna is quest index 4 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
        }

        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(4); /* Nienna is quest index 4 */

        /* Mark quest completion in metarun for score/persistence */
        metarun_mark_quest_completed(METARUN_QUEST_NIENA);
        log_trace("Nienna quest marked complete in metarun and Mercy oath unlocked");

        msg_print("Nienna smiles sadly and fades away, leaving you with her blessing.");

        /* Keep Nienna's custom farewell message without appending the
         * generic quest giver departure line.
         */
        remove_quest_giver_silent(R_IDX_NIENA);

        /* Recalculate bonuses to apply the new stealth bonus */
        p_ptr->update |= (PU_BONUS);
        handle_stuff();

        log_trace("Nienna quest completed and rewarded - stealth bonus: %d", stealth_bonus);
    }
}

/*
 * Check if player is adjacent to Nienna and handle interaction
 */
void check_niena_quest_interaction(void)
{
    /* Only check if quest is in appropriate state */
    if (p_ptr->niena_quest != NIENA_QUEST_GIVER_PRESENT &&
        p_ptr->niena_quest != NIENA_QUEST_SUCCESS)
    {
        return;
    }

    log_trace("check_niena_quest_interaction: checking adjacency, quest state: %d", p_ptr->niena_quest);

    if (trigger_adjacent_quest_giver_interaction(
        R_IDX_NIENA, "Nienna", niena_quest_interaction))
    {
        return;
    }

    if (p_ptr->niena_quest == NIENA_QUEST_SUCCESS)
        ensure_reward_quest_giver_near_player(
            R_IDX_NIENA, 3, "Nienna", NULL, NULL, NULL);
}

/*
 * Check if the player has completed the Nienna mercy quest by reaching down stairs
 */
void check_niena_quest_completion(void)
{
    /* Only check if quest is active */
    if (p_ptr->niena_quest != NIENA_QUEST_ACTIVE) {
        return;
    }

    /* Check if player is on down stairs */
    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE ||
        cave_feat[p_ptr->py][p_ptr->px] == FEAT_MORE_SHAFT) {

        log_trace("Player reached down stairs during Nienna quest - quest completed!");
        log_trace("Final counts: seen=%d, killed=%d", p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);

        p_ptr->niena_quest = NIENA_QUEST_SUCCESS;

        msg_print("As you step onto the stairs, you feel Nienna's presence return.");
        msg_print("'You have done well, showing mercy where others would show only violence.'");
        msg_print("Wait here a moment - she wishes to speak with you.");

        ensure_reward_quest_giver_near_player(
            R_IDX_NIENA, 3, "Nienna", NULL, NULL, NULL);
    }
}

/*
 * Handle interaction with Oromë for the hunting quest
 */
void orome_quest_interaction(void)
{
    /* Prevent multiple interactions in the same turn */
    static int last_interaction_turn = -1;
    if (last_interaction_turn == turn) {
        return; /* Already handled this turn */
    }
    last_interaction_turn = turn;

    /* Safety check - ensure valid quest state */
    if (p_ptr->orome_quest != OROME_QUEST_GIVER_PRESENT &&
        p_ptr->orome_quest != OROME_QUEST_SUCCESS)
    {
        log_trace("orome_quest_interaction called with invalid quest state: %d", p_ptr->orome_quest);
        return;
    }

    if (p_ptr->orome_quest == OROME_QUEST_GIVER_PRESENT)
    {
        if (!quest_can_accept_more()) {
            msg_print("You are already committed to two quests. Finish one before accepting another.");
            log_trace("Oromë quest: accept blocked by active quest cap (%d/%d)",
                      quest_accepted_count_this_run(), QUEST_MAX_ACCEPTED_PER_RUN);
            return;
        }

        log_trace("Starting Oromë quest interaction - offering hunting quest");

        /* Extract initialization texts from quest data */
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(5, &text_count); /* Oromë is quest index 5 */
        init_texts = prepend_repeat_context(QUEST_ID_OROME, init_texts, &text_count, false);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Oromë the Huntsman", init_texts, text_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(init_texts, text_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Oromë the Huntsman regards you with keen eyes:",
                "'Prove your skill as a hunter. The dark creatures multiply.'"
            };
            quest_typewriter_menu("Oromë the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
        }

        /* Determine hunt target based on dungeon depth */
        int depth = p_ptr->depth;
        cptr target_name;
        int target_count;

        if (depth <= 250) {
            p_ptr->orome_target_type = OROME_TARGET_WOLF;
            target_count = 100;
            target_name = "wolves";
        } else if (depth <= 500) {
            p_ptr->orome_target_type = OROME_TARGET_SPIDER;
            target_count = 80;
            target_name = "spiders";
        } else if (depth <= 750) {
            p_ptr->orome_target_type = OROME_TARGET_SERPENT;
            target_count = 60;
            target_name = "serpents";
        } else {
            p_ptr->orome_target_type = OROME_TARGET_VAMPIRE;
            target_count = 30;
            target_name = "vampires";
        }

        /* Accept the quest */
        p_ptr->orome_quest = OROME_QUEST_ACTIVE;
        p_ptr->orome_target_count = target_count;
        p_ptr->orome_killed_count = 0;

        /* Remove the quest giver now that quest is accepted without
         * showing the generic reward/departure message before the quest text.
         */
        remove_quest_giver_silent(R_IDX_OROME);

        msg_format("You must hunt and slay %d %s to prove your prowess.", target_count, target_name);
        msg_print("Return when the hunt is complete to claim your reward.");
        msg_print("Oromë fades into the wild, but his presence lingers in your soul.");

        log_trace("Oromë quest started - hunt %d %s at depth %d",
                 target_count, target_name, depth);
    }
    else if (p_ptr->orome_quest == OROME_QUEST_SUCCESS)
    {
        log_trace("Completing Oromë quest - giving Unique Bane reward");

        /* Extract completion texts from quest data */
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(5, &completion_count); /* Oromë is quest index 5 */
        completion_texts = prepend_repeat_context(QUEST_ID_OROME, completion_texts, &completion_count, true);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Oromë the Huntsman", completion_texts, completion_count, TERM_GREEN, TERM_WHITE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            /* Fallback to simple message if text extraction fails */
            cptr fallback_texts[] = {
                "Oromë appears with a proud smile!",
                "'You have proven yourself a true hunter of the wild.'"
            };
            quest_typewriter_menu("Oromë the Huntsman", fallback_texts, 2, TERM_GREEN, TERM_WHITE);
        }

        /* Show the specific numbers after the main dialogue */
        cptr monster_names[] = {"wolves", "spiders", "serpents", "vampires"};
        cptr monster_name = (p_ptr->orome_target_type >= 1 && p_ptr->orome_target_type <= 4)
                           ? monster_names[p_ptr->orome_target_type - 1] : "creatures";

        msg_format("You have slain %d %s as commanded.",
                  p_ptr->orome_target_count, monster_name);
        msg_print("You learn the secret of hunting unique creatures!");

        /* Clear quest state */
        p_ptr->orome_quest = OROME_QUEST_REWARDED;
        log_trace("Oromë reward: Quest state set to REWARDED (%d)", OROME_QUEST_REWARDED);

        /* Apply quest rewards from quest.txt data */
        apply_quest_rewards(5); /* Oromë is quest index 5 */

        /* Mark quest as completed in metarun */
        metarun_mark_quest_completed(METARUN_QUEST_OROME);

        /* Unlock oath for future characters in this metarun */
        int oath_id = get_quest_oath_id(5); /* Oromë is quest index 5 */
        if (oath_id > 0) {
            metarun_unlock_oath(oath_id);
            msg_format("The %s is now available for future characters in this lineage!",
                      get_oath_name_from_id(oath_id));
        }

        /* Remove the quest giver after giving reward */
        remove_quest_giver(R_IDX_OROME);

        log_trace("Oromë quest completed - Unique Bane granted, oath unlocked");
    }
}

/*
 * Check if player should interact with Oromë
 */
void check_orome_quest_interaction(void)
{
    /* Only check if quest can be started or completed */
    if (p_ptr->orome_quest != OROME_QUEST_GIVER_PRESENT &&
        p_ptr->orome_quest != OROME_QUEST_SUCCESS) {
        return;
    }

    log_trace("check_orome_quest_interaction: checking adjacency, quest state: %d", p_ptr->orome_quest);

    if (trigger_adjacent_quest_giver_interaction(
        R_IDX_OROME, "Oromë", orome_quest_interaction))
    {
        return;
    }

    if (p_ptr->orome_quest == OROME_QUEST_SUCCESS)
    {
        ensure_reward_quest_giver_near_player(
            R_IDX_OROME, 3, "Oromë",
            "Oromë the Huntsman materializes nearby, ready to honor your success!",
            NULL, NULL);
    }
}

/*
 * Grant the unique bane special ability to the player
 * This function can be called from quests, debug commands, or other rewards
 */
void grant_unique_bane_ability(void)
{
    log_trace("grant_unique_bane_ability: Function called, checking current state");
    log_trace("grant_unique_bane_ability: have_ability[S_SPC][SPC_UNIQUE_BANE] = %s",
             p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE");

    if (p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE])
    {
        log_trace("grant_unique_bane_ability: Player already has ability, showing message and returning");
        msg_print("You already possess the power to hunt unique creatures effectively.");
        return;
    }

    log_trace("grant_unique_bane_ability: Setting ability flags to TRUE");
    /* Grant the ability */
    p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] = true;
    p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE] = true;

    log_trace("grant_unique_bane_ability: After setting flags - have_ability=%s, active_ability=%s",
             p_ptr->have_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE",
             p_ptr->active_ability[S_SPC][SPC_UNIQUE_BANE] ? "TRUE" : "FALSE");

    msg_print("You have learned the art of Unique Bane!");
    msg_print("You gain significant advantages when fighting unique creatures.");

    log_trace("Granted Unique Bane special ability to player");

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);
    handle_stuff();
}
