---
applyTo: '**'
---

# Sil-More Quest System Memory

## Quest Logic Refactoring - Key Findings and Solutions

### Issue 1: Quest Vault Visibility Problem
**Problem**: Quest vaults report successful placement but are not visible to players.

**Investigation Findings**:
- Quest vault placement uses proper collision detection via `solid_rock()` function
- Vaults mark areas with `CAVE_ICKY` flag to prevent overlap with regular rooms  
- `build_vault()` correctly sets `CAVE_ROOM | CAVE_ICKY` on all vault tiles
- Quest vaults placed before regular room generation in level creation sequence
- Regular room generation (`room_build()`) uses same collision detection and should respect `CAVE_ICKY` areas

**Enhanced Debugging Added**:
- `process_quest_vault_area()` now logs detailed vault contents (walls, floors, features, monsters)
- Tracks exact coordinates and vault dimensions after placement
- Logs forge and quest NPC locations when found

**Possible Root Causes**:
1. Vault placement succeeds but vault content is somehow invisible/corrupted
2. Map display or update system not showing vault areas properly
3. Player starting position or movement restrictions preventing vault access
4. Vault generation working but quest NPCs/features not spawning correctly

### Issue 2: Metarun Quest Completion Persistence 
**Problem**: Quest completion does not persist across character runs.

**Investigation Findings**:
- `metarun_check_and_update_quests()` called from `update_stuff()` in `xtra1.c`
- Quest completion states: `TULKAS_QUEST_COMPLETE/REWARDED`, `AULE_QUEST_SUCCESS`, `MANDOS_QUEST_SUCCESS`
- `metarun_mark_quest_completed()` sets completion flags and calls `save_metaruns()`
- `save_metaruns()` called on game exit in `files.c` line 7651
- Metarun loading appears to work correctly (shows existing metarun data)

**Enhanced Debugging Added**:
- `metarun_check_and_update_quests()` now logs current quest states and completion detection
- Tracks when quests are marked as completed vs already completed
- Shows metarun current_run index and quest state values

**Possible Root Causes**:
1. Quest completion detection timing - checking before quest state is properly set
2. Save/load timing - metarun save happening before quest completion is recorded
3. Quest state reset - something clearing quest completion between detection and save
4. Metarun file corruption or save path issues

### Special Abilities System - Fixed Issues
**Problems Solved**:
- Special abilities not persisting: Fixed `calc_bonuses()` to preserve `S_SPC` abilities
- Special skill showing in menu: Fixed by hiding `S_SPC` in skill display functions
- Abilities menu showing all slots: Fixed `abilities_menu2()` to show only granted abilities

**Key Code Changes**:
- `xtra1.c`: Modified ability reset loop in `calc_bonuses()` to skip `S_SPC` 
- `cmd4.c`: Added filtering in `abilities_menu2()` to show only `have_ability[]` entries for `S_SPC`

### Quest Spawning Logic - Previous Fixes
**Quest Reservation System**:
- `p_ptr->quest_reserved[0]` prevents multiple quests per run
- Fixed quest vault placement to check `!p_ptr->quest_reserved[0]` 
- Metarun completion checking prevents spawning completed quests

## Files Modified
- `src/generate.c`: Enhanced quest vault debugging, fixed reservation checking
- `src/metarun.c`: Enhanced quest completion debugging  
- `src/xtra1.c`: Fixed special abilities persistence in `calc_bonuses()`
- `src/cmd4.c`: Fixed abilities menu filtering

## Testing Recommendations
1. **Quest Vault Testing**: Look for detailed vault content logs after level generation
2. **Metarun Testing**: Check quest completion logging during quest reward scenes
3. **Special Abilities**: Verify abilities persist after skill point allocation and character save/load
4. **Quest Spawning**: Ensure only one quest spawns per run and completed quests don't respawn

## Outstanding Issues to Monitor
1. Quest vault visibility - needs testing with enhanced debugging
2. Metarun quest completion persistence - needs testing across character runs
3. Potential room generation timing conflicts with quest vault placement
