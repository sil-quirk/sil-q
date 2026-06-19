#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "player/player-upkeep-internal.h"

/*
 * Handle "p_ptr->update"
 */
void update_stuff(void)
{
    u32b update_flags;

    /* Update stuff */
    if (!p_ptr->update) {
        // log_trace("update_stuff: no updates needed");
        return;
    }

    update_flags = p_ptr->update;
    log_trace("update_stuff: processing updates 0x%08X", p_ptr->update);

    if (p_ptr->update & (PU_BONUS))
    {
        p_ptr->update &= ~(PU_BONUS);
        // log_trace("update_stuff: calculating bonuses");
        calc_bonuses();
    }

    if (p_ptr->update & (PU_HP))
    {
        p_ptr->update &= ~(PU_HP);
        // log_trace("update_stuff: calculating hitpoints");
        calc_hitpoints();
    }

    if (p_ptr->update & (PU_MANA))
    {
        p_ptr->update &= ~(PU_MANA);
        // log_trace("update_stuff: calculating voice/mana");
        calc_voice();
    }

    /* Character is not ready yet, no screen updates */
    if (!character_generated) {
        // log_trace("update_stuff: character not generated yet, skipping screen updates");
        return;
    }

    // log_trace("update_stuff: character_icky=%d", character_icky);

    /* Character is in "icky" mode, no screen updates */
    if (character_icky) {
        // log_trace("update_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    if (p_ptr->update & (PU_FORGET_VIEW))
    {
        p_ptr->update &= ~(PU_FORGET_VIEW);
        log_trace("update_stuff: forgetting view");
        forget_view();
    }

    if (p_ptr->update & (PU_UPDATE_VIEW))
    {
        p_ptr->update &= ~(PU_UPDATE_VIEW);
        log_trace("update_stuff: updating view");
        update_view();

        /* Check artifact visibility after view update */
        check_artifact_visibility();
    }

    if (p_ptr->update & (PU_DISTANCE))
    {
        p_ptr->update &= ~(PU_DISTANCE);
        p_ptr->update &= ~(PU_MONSTERS);
        log_trace("update_stuff: updating distances and monsters");
        update_monsters(true);
    }

    if (p_ptr->update & (PU_MONSTERS))
    {
        p_ptr->update &= ~(PU_MONSTERS);
        update_monsters(false);
    }

    if (p_ptr->update & (PU_PANEL))
    {
        p_ptr->update &= ~(PU_PANEL);
        verify_panel();
    }

    /*
     * Lore used to run before the empty-update check, so every defensive
     * update_stuff() call scanned all dungeon objects. A single action reaches
     * this function several times. Run it only for a real update and give it
     * the original flags so it can coalesce repeated work from the same turn.
     */
    update_lore(update_flags);

    /* Check quest completion status for metarun tracking */
    // log_trace("update_stuff: About to call metarun_check_and_update_quests()");
    metarun_check_and_update_quests();
    // log_trace("update_stuff: Finished calling metarun_check_and_update_quests()");

    // log_trace("update_stuff: completed all updates");
}
