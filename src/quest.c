/* Quest tracking helpers split from metarun.c */
#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include <string.h>

/* Local helpers */
static int popcount32(u32b value)
{
    int count = 0;
    while (value) {
        value &= (value - 1);
        count++;
    }
    return count;
}

static const u32b metarun_known_quest_flags[] = {
    METARUN_QUEST_TULKAS,
    METARUN_QUEST_AULE,
    METARUN_QUEST_MANDOS,
    METARUN_QUEST_NIENA,
    METARUN_QUEST_OROME,
    METARUN_QUEST_VARDA
};

#define METARUN_KNOWN_QUEST_MASK (METARUN_QUEST_TULKAS | METARUN_QUEST_AULE | METARUN_QUEST_MANDOS | METARUN_QUEST_NIENA | METARUN_QUEST_OROME | METARUN_QUEST_VARDA)

static int quest_slot_from_flag(u32b quest_flag)
{
    for (size_t i = 0; i < N_ELEMENTS(metarun_known_quest_flags) && i < METARUN_QUEST_SLOT_MAX; i++) {
        if (quest_flag == metarun_known_quest_flags[i]) return (int)i;
    }
    return -1;
}

void metarun_seed_quest_counts_from_mask(metarun *m, u32b mask)
{
    if (!m) return;
    for (size_t i = 0; i < N_ELEMENTS(metarun_known_quest_flags) && i < METARUN_QUEST_SLOT_MAX; i++) {
        m->quest_completion_counts[i] = (mask & metarun_known_quest_flags[i]) ? 1 : 0;
    }
    for (size_t i = N_ELEMENTS(metarun_known_quest_flags); i < METARUN_QUEST_SLOT_MAX; i++) {
        m->quest_completion_counts[i] = 0;
    }
}

void metarun_clamp_and_sync_quests(metarun *m)
{
    if (!m) return;

    u32b mask = m->completed_quests & ~((u32b)METARUN_KNOWN_QUEST_MASK);

    for (size_t i = 0; i < METARUN_QUEST_SLOT_MAX; i++) {
        byte count = m->quest_completion_counts[i];
        if (count > METARUN_QUEST_COMPLETION_CAP) {
            count = METARUN_QUEST_COMPLETION_CAP;
        }

        if (i < N_ELEMENTS(metarun_known_quest_flags)) {
            if (count > 0) {
                mask |= metarun_known_quest_flags[i];
            }
        } else {
            count = 0;
        }

        m->quest_completion_counts[i] = count;
    }

    m->completed_quests = mask;
}

int metarun_total_quest_completions(const metarun *m)
{
    if (!m) return 0;

    int total = 0;
    for (size_t i = 0; i < METARUN_QUEST_SLOT_MAX && i < N_ELEMENTS(metarun_known_quest_flags); i++) {
        total += m->quest_completion_counts[i];
    }

    /* Preserve completions for unknown future quests represented only by the bitmask */
    u32b unknown_mask = m->completed_quests & ~((u32b)METARUN_KNOWN_QUEST_MASK);
    total += popcount32(unknown_mask);

    return total;
}

#define QUEST_RESERVED_RECORD_BASE 1

static bool quest_completion_recorded_for_run(u32b quest_flag)
{
    if (!p_ptr) return false;
    int slot = quest_slot_from_flag(quest_flag);
    if (slot < 0) return false;

    int idx = QUEST_RESERVED_RECORD_BASE + slot;
    if (idx >= (int)N_ELEMENTS(p_ptr->quest_reserved)) return true; /* fail safe: assume recorded */

    return p_ptr->quest_reserved[idx] != 0;
}

static void mark_quest_completion_recorded_for_run(u32b quest_flag)
{
    if (!p_ptr) return;
    int slot = quest_slot_from_flag(quest_flag);
    if (slot < 0) return;

    int idx = QUEST_RESERVED_RECORD_BASE + slot;
    if (idx >= (int)N_ELEMENTS(p_ptr->quest_reserved)) return;

    p_ptr->quest_reserved[idx] = 1;
}

int metarun_quest_completion_count(u32b quest_flag)
{
    if (metarun_current_index() < 0) return 0;

    int slot = quest_slot_from_flag(quest_flag);
    if (slot >= 0 && slot < METARUN_QUEST_SLOT_MAX) {
        return metar.quest_completion_counts[slot];
    }

    /* Unknown flags fall back to the bitmask so legacy callers still work */
    return (metar.completed_quests & quest_flag) ? 1 : 0;
}

bool metarun_is_quest_completed(u32b quest_flag)
{
    /* Only check the current metarun, not all metaruns */
    const metarun *current = metarun_current();
    s16b current_idx = metarun_current_index();
    if (!current) {
        log_trace("Metarun quest check: Invalid current run (idx=%d, max=%d)", current_idx, metarun_entry_count());
        return false;
    }

    int count = metarun_quest_completion_count(quest_flag);
    if (count > 0) {
        log_trace("Metarun quest check: Quest 0x%x completed %d time(s) in metarun[%d] (id=%d)",
                  quest_flag, count, current_idx, current->id);
        return true;
    }

    log_trace("Metarun quest check: Quest 0x%x not completed in current metarun[%d] (id=%d)",
              quest_flag, current_idx, current->id);
    return false;
}

void metarun_mark_quest_completed(u32b quest_flag)
{
    metarun *current = metarun_current_mutable();
    if (!current) return;
    if (!quest_flag) return;

    int slot = quest_slot_from_flag(quest_flag);
    bool changed = false;

    if (slot >= 0 && slot < METARUN_QUEST_SLOT_MAX) {
        byte current = metar.quest_completion_counts[slot];
        if (current < METARUN_QUEST_COMPLETION_CAP) {
            metar.quest_completion_counts[slot] = current + 1;
            changed = true;
        }
        /* Always sync the per-run marker so we don't double-count this character */
        mark_quest_completion_recorded_for_run(quest_flag);
    } else if (!(metar.completed_quests & quest_flag)) {
        /* Unknown quest flag - preserve legacy behavior */
        metar.completed_quests |= quest_flag;
        changed = true;
    }

    metarun_clamp_and_sync_quests(&metar);
    current->completed_quests = metar.completed_quests;
    memcpy(current->quest_completion_counts, metar.quest_completion_counts, sizeof(metar.quest_completion_counts));

    if (changed) {
        int new_count = metarun_quest_completion_count(quest_flag);
        log_trace("Metarun: Quest 0x%x completion recorded (count=%d, mask=0x%08X)",
                  quest_flag, new_count, metar.completed_quests);
        refresh_current_metar_score();
        save_metaruns();
    } else {
        log_trace("Metarun: Quest 0x%x completion ignored (already at cap or recorded); mask=0x%08X",
                  quest_flag, metar.completed_quests);
    }
}

void metarun_check_and_update_quests(void)
{
    s16b current_idx = metarun_current_index();
    log_trace("Metarun quest check: Entry - current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
    
    if (current_idx < 0 || !p_ptr) {
        log_trace("Metarun quest check: Early return - current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
        return;
    }
    
    log_trace("Metarun quest check: current_run=%d, tulkas=%d, aule=%d, mandos=%d, niena=%d, orome=%d, varda=%d", 
              current_idx, p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->varda_quest);

    /* Only record once per character; completion handlers also call metarun_mark_quest_completed */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_TULKAS)) {
        log_trace("Metarun: Marking Tulkas quest as completed (rewarded, was %d)", p_ptr->tulkas_quest);
        metarun_mark_quest_completed(METARUN_QUEST_TULKAS);
    }
    
    if (p_ptr->aule_quest == AULE_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_AULE)) {
        log_trace("Metarun: Marking Aule quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_AULE);
    }

    if (p_ptr->mandos_quest == MANDOS_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_MANDOS)) {
        log_trace("Metarun: Marking Mandos quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_MANDOS);
    }

    if (p_ptr->niena_quest == NIENA_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_NIENA)) {
        log_trace("Metarun: Marking Nienna quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_NIENA);
    }

    if (p_ptr->orome_quest == OROME_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_OROME)) {
        log_trace("Metarun: Marking Orome quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_OROME);
    }
    
    if (p_ptr->varda_quest == VARDA_QUEST_REWARDED && !quest_completion_recorded_for_run(METARUN_QUEST_VARDA)) {
        log_trace("Metarun: Marking Varda quest as completed (rewarded)");
        metarun_mark_quest_completed(METARUN_QUEST_VARDA);
    }
}

void metarun_restore_quest_states(void)
{
    s16b current_idx = metarun_current_index();
    const metarun *current = metarun_current();
    if (!current) {
        log_trace("Metarun restore: Invalid current_run=%d, metarun_max=%d", current_idx, metarun_entry_count());
        return;
    }
    
    log_trace("Metarun restore: Restoring quest states from metarun[%d], completed_quests=0x%08X", 
              current_idx, current->completed_quests);
    
    /* Restore Tulkas quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_TULKAS) > 0) {
        if (p_ptr->tulkas_quest < TULKAS_QUEST_REWARDED) {
            p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
            log_trace("Metarun restore: Tulkas quest set to REWARDED (%d)", TULKAS_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_TULKAS);
    }
    
    /* Restore Aule quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_AULE) > 0) {
        if (p_ptr->aule_quest < AULE_QUEST_REWARDED) {
            p_ptr->aule_quest = AULE_QUEST_REWARDED;
            log_trace("Metarun restore: Aule quest set to REWARDED (%d)", AULE_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_AULE);
    }
    
    /* Restore Mandos quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_MANDOS) > 0) {
        if (p_ptr->mandos_quest < MANDOS_QUEST_REWARDED) {
            p_ptr->mandos_quest = MANDOS_QUEST_REWARDED;
            log_trace("Metarun restore: Mandos quest set to REWARDED (%d)", MANDOS_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_MANDOS);
    }
    
    /* Restore Niena quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_NIENA) > 0) {
        if (p_ptr->niena_quest < NIENA_QUEST_REWARDED) {
            p_ptr->niena_quest = NIENA_QUEST_REWARDED;
            p_ptr->niena_level = 0; /* Clear depth for previous run attribution */
            log_trace("Metarun restore: Niena quest set to REWARDED (%d)", NIENA_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_NIENA);
    }
    
    /* Restore Orome quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_OROME) > 0) {
        if (p_ptr->orome_quest < OROME_QUEST_REWARDED) {
            p_ptr->orome_quest = OROME_QUEST_REWARDED;
            log_trace("Metarun restore: Orome quest set to REWARDED (%d)", OROME_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_OROME);
    }
    
    /* Restore Varda quest state */
    if (metarun_quest_completion_count(METARUN_QUEST_VARDA) > 0) {
        if (p_ptr->varda_quest != VARDA_QUEST_REWARDED) {
            p_ptr->varda_quest = VARDA_QUEST_REWARDED;
            log_trace("Metarun restore: Varda quest set to REWARDED (%d)", VARDA_QUEST_REWARDED);
        }
        mark_quest_completion_recorded_for_run(METARUN_QUEST_VARDA);
    }
    
    log_trace("Metarun restore: Final quest states - Tulkas: %d, Aule: %d, Mandos: %d, Niena: %d, Orome: %d, Varda: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->niena_quest, p_ptr->orome_quest, p_ptr->varda_quest);
}
