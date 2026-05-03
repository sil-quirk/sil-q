/*
 * File: thrall_quest.h
 * Purpose: Thrall quest system - alert thralls can request items from the player
 *          and provide selectable rewards
 */

#ifndef THRALL_QUEST_H
#define THRALL_QUEST_H

#include "angband.h"

/*
 * Check if a monster is an alert thrall
 */
bool is_alert_thrall(monster_type* m_ptr);

/*
 * Initialize a thrall's quest (assign what item they want)
 * Should be called when an alert thrall is spawned
 */
void init_thrall_quest(monster_type* m_ptr);

/*
 * Get the name of the item the thrall wants
 */
cptr get_thrall_quest_item_name(byte quest_item);

/*
 * Check if player has the requested item in inventory
 * Returns the inventory slot if found, or SUPPLIES_INDEX + supply_idx for
 * supplies items, -1 otherwise.
 */
int player_has_thrall_quest_item(byte quest_item);

/*
 * Handle interaction when player bumps into an alert thrall
 * Returns true if an action was taken (quest completed or in progress)
 */
bool handle_thrall_interaction(monster_type* m_ptr);

/*
 * Complete the thrall's quest - consume the item and offer the reward
 */
void complete_thrall_quest(monster_type* m_ptr, int item_slot);

/*
 * Check whether an object currently carries the DAMAGED flag.
 */
bool object_is_damaged_item(const object_type* o_ptr);

/*
 * Check whether an object's damage can actually be repaired by the repair flow.
 */
bool object_can_repair_damage(const object_type* o_ptr);

/*
 * Find a damaged item in player's inventory/equipment that can be repaired
 * Returns the slot if found, -1 otherwise
 */
int find_broken_item_to_upgrade(void);

/*
 * Repair a damaged item without showing the thrall reward presentation.
 */
bool repair_damaged_item(int slot);

/*
 * Upgrade a broken item to its normal version, keeping special properties
 */
bool upgrade_broken_item(int slot);

/*
 * Reveal random unknown artifact descriptions
 * Returns true if at least one artifact was revealed
 */
bool reveal_random_artifact(void);

#endif /* THRALL_QUEST_H */
