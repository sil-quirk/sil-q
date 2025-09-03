---
applyTo: '**'
---

# Sil-More Project Memory & Knowledge Base

## Build System Notes
**IMPORTANT**: Use `Makefile.cyg` for compilation, NOT `Makefile.win`
- Command: `make -f Makefile.cyg` (in Cygwin bash)
- Makefile.win has linking issues and should be avoided
- Always use Cygwin environment for proper compilation

## Log File Location
**CRITICAL**: The log.txt file location depends on WHERE you run the executable:
- If run from `/cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/` → log.txt appears in root directory
- If run from `/cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src/` → log.txt appears in src directory  
- Always check the correct location based on where sil.exe was executed

## Oath System - Complete Implementation & Recent Bug Fixes

### Overview
The oath system has been converted from Will abilities to Special abilities that unlock through quest completion and are selected at character birth.

### System Architecture
**Data Storage**: 
- `metarun.unlocked_oaths` - tracks which oaths are available (bit mask)
- `metarun.banned_oaths` - tracks which oaths are permanently banned (bit mask)
- `p_ptr->oath_type` - current character's selected oath (1=Mercy, 2=Silence, 3=Iron)

**Special Abilities**:
- `SPC_OATH_MERCY` (2) - grants +1 Grace, breaks on killing helpless foes
- `SPC_OATH_SILENCE` (3) - grants +1 Strength, breaks on singing
- `SPC_OATH_IRON` (4) - grants +2 Constitution, breaks on fleeing without Silmaril

### Major Bug Fixes Completed (August 2025)

#### Issue #1: Character Screen Bug in Oath Breaking
**Problem**: When breaking an oath, character movement screen was broken due to `character_icky` imbalance
**Solution**: Added missing `screen_load()` call in `choose_escape_curses_ui()` function in `src/metarun.c`
**Status**: ✅ FIXED

#### Issue #2: Curse Selection Count 
**Problem**: When breaking oath, player was getting all 3 curses instead of choosing 1 out of 3
**Solution**: Changed parameter from 3 to 1 in `choose_escape_curses_ui(1, chosen_curses)` call in `src/cmd1.c`
**Status**: ✅ FIXED

#### Issue #3: Duplicate Death Saves
**Problem**: Multiple save operations during character death were breaking metarun logic
**Solution**: Added static flag protection with proper cleanup in `close_game_aux()` function in `src/files.c`
**Status**: ✅ FIXED - Critical bug affecting suicide saves and score database entry

#### Issue #4: Ability Menu Navigation
**Problem**: Menu navigation skipped letters due to using total ability count instead of visible count
**Solution**: Comprehensive refactoring of `abilities_menu2()` function in `src/cmd4.c`:
- Added `visible_count` tracking for sequential letter assignment
- Added `visible_abilities[]` array to map display letters to actual ability indices  
- Updated arrow key navigation to use `visible_count` instead of `options`
- Fixed key handling to properly map letters to visible abilities
**Status**: ✅ FIXED

#### Issue #5: Smith Oath Menu Bug & UI Redesign (September 2025)
**Problem**: 
1. Smith oath not appearing in oath changing menu due to OATH_TYPES=4 but arrays missing Smith entries
2. Poor UI design that doesn't scale to 10+ oaths 
3. Text positioning issues with overlapping text
4. Layout doesn't work well in 80x25 terminal constraints

**Root Cause Analysis**: 
- Original oath menu used simple vertical layout with hardcoded positioning
- Didn't follow established UI patterns from the codebase
- Used `Term_clear()` instead of proper partial screen clearing
- Wrong column positioning causing text overlap

**Solution - Proper UI Redesign**:
1. **Fixed Data Issues**:
   - Updated OATH_TYPES from 4 to 5 
   - Added Smith oath entries to all arrays (oath_name, oath_desc1, oath_desc2, oath_reward)
   - **CRITICAL FIX**: Fixed `oath_restrictions` array in birth.c from 4 to 5 entries
   - Added Smith oath restriction: "You may not pick up weapons or armour from the ground"

2. **Complete UI Redesign Following Established Patterns**:
   - Studied abilities_menu2() function as the reference implementation
   - Used proper three-column layout:
     * COL_SKILL (2): Unused (available for future categories)
     * COL_ABILITY (17): Compact oath list, 1 line per oath  
     * COL_DESCRIPTION (42): Detailed descriptions with text wrapping
   - Used `wipe_screen_from(COL_ABILITY)` instead of `Term_clear()`
   - Proper color coding: TERM_L_RED for broken, TERM_L_BLUE for highlighted
   - Text wrapping for long descriptions to fit 38-character column width

3. **User Experience Improvements**:
   - **Filtering**: Only show available and broken oaths (hide locked ones per user request)
   - **Input Mapping**: Proper letter selection with display position → actual oath index translation
   - **Navigation**: Filtered navigation skipping locked oaths completely
   - **Dual Menu Support**: Fixed both cmd4.c (abilities) and birth.c (character creation) menus

4. **Scalability Achievements**:
   - Supports up to 20 oaths in 80x25 terminal (rows 4-23)
   - Clean separation prevents text overlap
   - Easily extensible for scrolling if >20 oaths needed
   - Could add oath categories in COL_SKILL column if desired

**Status**: ✅ COMPLETELY FIXED 

**Final Resolution Summary:**
1. **Root Cause Identified**: User was seeing birth.c `select_oath()` function, not cmd4.c `oath_menu()` 
2. **Three-Column Design Applied**: Converted birth.c oath menu to proper three-column layout
3. **Array Bounds Fixed**: Added missing Smith oath to `oath_restrictions[5]` array  
4. **Filtering Implemented**: Only show available and broken oaths (hide locked ones)
5. **Debug Logging Added**: Track oath ability assignments for troubleshooting
6. **Both Menus Fixed**: cmd4.c (abilities) and birth.c (character creation) now use proper UI

**Current State**: 
- ✅ New three-column oath menu displays during character creation after skill assignment
- ✅ Smith oath shows correct restriction: "You may not pick up weapons or armour from the ground"  
- ✅ Filtered display shows only available/broken oaths as requested
- ✅ Proper debugging added to track oath ability activation issues
- ✅ Scalable design supports 10+ oaths with proper text wrapping

## Build Environment & Compilation

### Windows Build Setup
- **Operating System**: Windows with Cygwin environment
- **Compiler**: MinGW-w64 GCC via Cygwin
- **Terminal**: Use Cygwin bash terminal for compilation
- **Makefile**: Use `Makefile.cyg` for Windows builds

### Compilation Commands
```bash
# Navigate to source directory
cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src

# Clean build
make -f Makefile.cyg clean

# Full build
make -f Makefile.cyg

# Parallel build (faster)
make -f Makefile.cyg -j8

# Build and launch
make -f Makefile.cyg -j8 launch

# Compile single file for testing
make -f Makefile.cyg generate.o
```

### Quest System Architecture & Rules

### Quest Spawning Logic - Critical Design Pattern
**Rule**: Spawning quests (Tulkas, Niena, etc.) use a **lottery-then-persist** approach:
1. Each quest gets evaluated **exactly once** in a predetermined order
2. If a quest wins its probability roll, it "claims" the level and we regenerate until its requirements are met
3. Once a quest claims a level, no other quests are evaluated - we keep regenerating for that quest only
4. This prevents simultaneous spawning and eliminates regeneration conflicts

**Implementation Pattern**:
- Use `quest_reserved[0]` to track which quest has claimed the level
- Set reservation BEFORE attempting to place the quest 
- During regeneration, preserve the reservation and only attempt the claiming quest
- Only reset reservation when moving to a new level

**Quest Probabilities**:
- **Tulkas**: `1/(25-depth)` for depths 6-19 (minimum level 6)
- **Niena**: `p_Nienna(lvl) = 0.125 * max(0, min(1, (lvl - 14) / 5))` for depths 14+ (scales from 0% at 14 to 12.5% at 19+)

**Vault Quests**: Have different logic and don't use this lottery system (they have fixed locations)
**Solution**: Modified `oath_description()` in `src/metarun.c` to display "FORBIDDEN (OATH BROKEN)" for banned oaths
**Status**: ✅ FIXED

## Niena Quest System - Major Enhancements (January 2025)

### Overview
The Niena mercy quest system was significantly enhanced to improve consistency with other quests and provide more balanced rewards.

### Changes Implemented

#### Spawning System Update
**Original**: Niena spawned near up stairs within 10 tile radius
**New**: Niena spawns near player within 2 square radius (same as Tulkas)
**Location**: `generate.c` around line 5040
**Status**: ✅ COMPLETE

#### Special Ability Reward System
**Original**: Simple `10 * (seen - killed)` stealth bonus  
**New**: Formula-based special ability - `10 * (seen - killed) / seen` rounded up, maximum 10
**Implementation**:
- Added `SPC_NIENA_MERCY` constant (6) in `defines.h`
- Added "Niena's Gift of Mercy" ability definition in `lib/edit/ability.txt`
- Updated calculation in `xtra1.c` with ceiling division formula
- Updated quest reward system in `xtra2.c` to grant new special ability
**Status**: ✅ COMPLETE

### Win Condition System - Documentation

#### Core Mechanics
**Silmaril Requirement**: To escape from Morgoth's throne room (depth 1000), player must possess at least 1 Silmaril
**Escape Process**: Player must reach depth 0 (Gates of Angband) with Silmarils to complete the game
**Victory Levels**: 
- 0 Silmarils: Escaped empty handed
- 1 Silmaril: Brought back one Silmaril from Morgoth's crown  
- 2 Silmarils: Brought back two Silmarils from Morgoth's crown
- 3 Silmarils: Brought back all three Silmarils from Morgoth's crown (perfect victory)

#### Shaft Mechanics in Win Conditions
**Up Shaft Behavior**: 
- Normal stairs: Move up 1 level, create `FEAT_MORE` return stair
- Shaft stairs (`FEAT_LESS_SHAFT`): Move up 2 levels, create `FEAT_MORE_SHAFT` return stair
- From Morgoth's room: Shaft stairs work normally if player has Silmaril(s)

**Down Shaft Behavior**:
- Normal stairs: Move down 1 level, create `FEAT_LESS` return stair  
- Shaft stairs (`FEAT_MORE_SHAFT`): Move down 2 levels, create `FEAT_LESS_SHAFT` return stair
- Cannot descend from Gates (depth 0) - game prevents re-entry

**Trapped Stairs**: Can cause falling damage and send player deeper instead of intended direction

**Critical Rule**: Without Silmarils, player cannot leave Morgoth's throne room via any stair type - they get "maze of staircases" message and remain trapped.
**Solution**: Modified `list_oaths_ui()` function in `src/metarun.c` to:
- Display broken oaths with red coloring and forbidding text
- Clear warning text about consequences of choosing broken oaths
- Allow selection but show proper warning messages
**Status**: ✅ FIXED

## Quest System - Niena Mercy Quest Implementation (August 2025)

### Overview
Implemented comprehensive quest system for Niena mercy quest with pacifist mechanics, including critical fixes for regeneration logic and interaction systems.

### Critical Fixes Applied

#### Issue #1: Quest State Reset Logic
**Problem**: Niena quest was being reset from GIVER_PRESENT to NOT_STARTED during regeneration
**Root Cause**: Incorrect level-based condition in reset logic (treated like vault quests instead of entrance quests)
**Solution**: Changed to unconditional reset for entrance-based quests like Tulkas in `generate.c` reset_quest_vault_states()
**Status**: ✅ FIXED

#### Issue #2: Single-Roll Dice Logic  
**Problem**: Niena quest dice was being rolled multiple times during regeneration instead of once
**Root Cause**: `roll_for_niena_quest()` called inside `cave_gen()` which is in the regeneration loop
**Solution**: 
- Moved Niena dice roll to `generate_cave()` before regeneration loop
- Removed flag reset from `cave_gen()`
- Uses minimum possible level size for consistent quest decision across regeneration attempts
**Location**: `generate.c` - moved from inside regeneration loop to before it
**Status**: ✅ FIXED

#### Issue #2b: Niena Spawning vs Quest State **ACTIVE DEBUGGING**
**Problem**: Niena quest conditions are met (dice roll success, stair separation pass) but she doesn't actually spawn or interact
**Analysis**: The "Level generation SUCCESS" message is misleading - it only indicates overall level generation completed, NOT that Niena spawned
**Root Causes Being Investigated**:
- Spawning code may not be reached due to failing conditions
- Room detection timing issues (spawning requires properly marked rooms)
- Quest state persistence during regeneration (monsters cleared but quest state remains)
**Debug Changes Applied**: Added comprehensive condition logging to identify which spawn requirement fails
**Status**: 🔍 **ACTIVE INVESTIGATION** - Debug logging added, awaiting test results

#### Issue #3: Quest Failure Detection
**Problem**: No quest failure when leaving level via stairs
**Solution**: Added quest reset and failure message in `do_cmd_go_up()` function
**Location**: `cmd2.c` around line 300
**Status**: ✅ FIXED

#### Issue #4: Stair Separation Performance  
**Problem**: Harsh 50% diagonal stair separation causing excessive regeneration
**Solution**: Reduced to 30% of diagonal distance requirement
**Impact**: Improved level generation performance significantly
**Status**: ✅ FIXED

### Technical Architecture
- **Entrance-based quests** (Tulkas, Niena): Spawn during generation, no level persistence needed
- **Vault-based quests** (Aule, Mandos): Use pending states system for cross-regeneration persistence  
- **Single-roll system**: Dice thrown once at level start, decision persists through all regeneration attempts
- **Quest constants**: Properly defined in defines.h with clear state progression

### Quest System Status
All critical issues reported by user have been addressed:
1. ✅ Niena interaction issue (quest state reset problem)
2. ✅ Multiple dice rolling during regeneration  
3. ✅ Quest failure detection for level exit
4. ✅ Performance optimization (stair separation)
**Solution**: Enhanced oath selection in `src/birth.c`:
- Display broken oaths with "(BROKEN)" label in red text
- Show menacing Tolkien-style descriptions when broken oaths are highlighted
- Allow navigation to broken oaths (but prevent selection)
- Fixed UI layout issues with button positioning
**Status**: ✅ FIXED

#### Issue #6: Tulkas Quest Artifact Rewards
**Problem**: `valar_reserved_artifacts` array was NULL, preventing artifact rewards
**Solution**: Added safety initialization in Tulkas quest code when array is NULL
**Status**: ✅ FIXED - Now initializes array if missing during quest interactions

### Oath Lifecycle
1. **Quest Completion**: Unlocks oath for future characters (`metarun_unlock_oath()`)
2. **Birth Selection**: Choose from available oaths, grants special ability
3. **Active Benefits**: Stat bonuses applied via special abilities
4. **Breaking**: Disables special ability, applies random curse, bans oath permanently

### Quest-to-Oath Mapping
- Tulkas Quest → Oath of Silence (no corresponding special ability, artifact reward only)
- Aule Quest → Oath of Mercy (corresponds to Aule's Forge)
- Mandos Quest → Oath of Iron (corresponds to Mandos' Doom)

## Quest System Architecture - Complete Analysis

### Overview
The Sil-More quest system supports two distinct quest types with different spawning mechanisms and lifecycle management. All quests integrate with a metarun system to prevent respawning across character deaths.

### Quest Types

#### 1. Vault-Based Quests (Aule, Mandos)
**Spawning Method**: Special vault templates placed during level generation
**State Management**: Deferred via `pending_quest_states` system
**Level Storage**: Yes (`aule_level`, `mandos_level`)
**States**: NOT_STARTED(0) → GIVER_PRESENT(1) → ACTIVE(2) → SUCCESS(3) → REWARDED(4)

**Technical Details**:
- Placed in `try_quest_vault_type()` during early level generation
- Use `pending_quest_states` to defer state changes until successful generation
- Quest states only applied via `apply_pending_quest_states()` on successful level completion
- Reset during regeneration via `reset_quest_vault_states()` if placed at current level

#### 2. Entrance-Based Quests (Tulkas)
**Spawning Method**: Monster placement during level generation
**State Management**: Immediate state application
**Level Storage**: No (spawns at current level)
**States**: NOT_STARTED(0) → GIVER_PRESENT(1) → ACTIVE(2) → COMPLETE(3) → REWARDED(4)

**Technical Details**:
- Spawned in monster placement phase via probability-based placement
- Quest state set immediately when monster is placed
- No deferred state system needed
- Reset during regeneration if in GIVER_PRESENT state

### Quest Reservation System
**Purpose**: Prevent multiple quests per character run
**Mechanism**: `p_ptr->quest_reserved[0]` flag
**Set When**: Immediately upon quest placement/spawning
**Reset When**: Level regeneration, new character creation
**Scope**: Per-character run (not persistent across deaths)

### Metarun Integration
**Purpose**: Prevent quest respawning across character deaths
**Flags**: METARUN_QUEST_TULKAS, METARUN_QUEST_AULE, METARUN_QUEST_MANDOS, METARUN_QUEST_NIENA
**Storage**: Persistent file system (`metaruns.dat`)
**Scope**: Until maximum deaths or silmarils achieved

### Tulkas Quest Enhancement
**Target Level**: +5 levels to target monster for increased difficulty
**Artifact Reward**: Enhanced artifacts for more challenging targets

### Level Generation Integration

#### Phase 1: Quest Vault Placement
- **When**: Before regular room generation
- **Function**: `try_quest_vault_type()`
- **Checks**: Skill requirements, metarun completion, quest reservation
- **Action**: Immediate `quest_reserved[0] = 1`, deferred state via `pending_quest_states`

#### Phase 2: Monster Placement
- **When**: After room generation
- **Function**: Tulkas spawn logic in monster placement
- **Checks**: Depth range, metarun completion, quest reservation
- **Action**: Immediate quest state and reservation setting

#### Phase 3: State Application
- **When**: Successful level generation completion
- **Function**: `apply_pending_quest_states()`
- **Action**: Apply deferred vault-based quest states

#### Regeneration Handling
- **When**: Level generation fails and retries
- **Function**: `reset_quest_vault_states()`
- **Action**: Reset all quest states that were set during current level generation

### Quest State Lifecycle

#### Aule Quest (Smithing)
1. **NOT_STARTED**: Default state
2. **FORGE_PRESENT**: Forge placed in vault, can be interacted with
3. **ACTIVE**: Player has started smithing at forge
4. **SUCCESS**: Player completes smithing requirement
5. **REWARDED**: Special ability granted, metarun marked complete

#### Mandos Quest (Doom)
1. **NOT_STARTED**: Default state
2. **GIVER_PRESENT**: Mandos NPC placed in vault
3. **ACTIVE**: Player accepts quest from Mandos
4. **SUCCESS**: Player completes quest objectives
5. **REWARDED**: Special ability granted, metarun marked complete

#### Tulkas Quest (Champion)
1. **NOT_STARTED**: Default state
2. **GIVER_PRESENT**: Tulkas NPC spawned on level
3. **ACTIVE**: Player accepts quest from Tulkas
4. **COMPLETE**: Player defeats assigned target
5. **REWARDED**: Artifact granted, metarun marked complete

#### Niena Quest (Mercy)
1. **NOT_STARTED**: Default state
2. **GIVER_PRESENT**: Niena NPC spawned on maximum-size level with sufficient stair distance
3. **ACTIVE**: Player accepts mercy quest from Niena, all stairs become visible
4. **SUCCESS**: Player reaches down stairs without killing any monsters during quest
5. **REWARDED**: Special stealth ability granted based on mercy shown (seen vs killed monsters)

**Spawn Conditions**: 
- Level must be maximum size (parameter l >= 5, dimensions 55x165)
- **TESTING MODE**: 1-in-1 (100%) chance for Niena attempt on maximum-size levels
- **REBALANCED**: Tulkas formula changed from 1/(20-depth) to 1/(30-depth) to reduce late-game dominance
- If selected, distance between up stairs (spawn) and nearest down stairs >= half diagonal of level
- Quest not already completed in metarun (`METARUN_QUEST_NIENA`)
- No other quest active (`quest_reserved[0] == 0`)

**Quest Mechanics**:
- **Monster Tracking**: Counts all monsters seen vs killed during active quest
- **Pacifist Objective**: Must reach down stairs without killing any monsters
- **Dual Rewards**: Grants both `SPC_NIENA` special ability (stealth bonus = 10 * (seen - killed)) AND unlocks Mercy oath for future characters
- **Stair Visibility**: All stairs on level become permanently visible when quest starts
- **Restriction**: Niena monster excluded from spawn outside quests (`SPECIAL_GEN` flag)

**Implementation Details**:
- **Quest Tracking**: `p_ptr->niena_quest` state, `p_ptr->niena_monsters_seen/killed` counters
- **Monster Placement**: Level generation phase checks conditions and spawns Niena near up stairs
- **Visibility Tracking**: Hooks into `update_mon()` in monster2.c for first-time visibility
- **Death Tracking**: Hooks into `monster_death()` in xtra2.c for kill counting
- **Completion**: Detected when player steps on down stairs during active quest
- **Metarun Integration**: Uses `metarun_mark_quest_completed(METARUN_QUEST_NIENA)` for persistence
- **Regeneration Integration**: Fully integrated with existing quest regeneration system
  - Quest states reset in `reset_quest_vault_states()` during failed level generation
  - **CRITICAL FIX**: Uses single-roll approach - Niena dice rolled ONCE at start of `cave_gen()`
  - Attempt flag (`niena_was_attempted`) persists across regeneration cycles until level accepted
  - Uses existing `cave_gen()` return value system to force regeneration when stair separation inadequate
  - Participates in `quest_reserved[0]` system like other quests
  - **Prevents infinite loops**: If conditions are harsh, keeps trying same rolled decision rather than re-rolling

### File Structure

#### Core Files
- **src/generate.c**: Level generation, quest placement, regeneration logic
- **src/xtra2.c**: Quest interactions, reward systems
- **src/metarun.c**: Persistent quest completion tracking
- **src/defines.h**: Quest state constants

#### Key Functions
- `try_quest_vault_type()`: Vault-based quest placement
- `reset_quest_vault_states()`: Regeneration quest state cleanup
- `apply_pending_quest_states()`: Deferred state application
- `metarun_check_and_update_quests()`: Completion tracking

### Recent Fixes Applied

#### FIXED ISSUE 9: Regeneration Logic - RESOLVED ✅

**PROBLEM**: Quest reservation not reset during level regeneration, blocking quest vault placement
- **Root Cause**: Complex interaction between immediate reservation and pending quest state system
- **Specific Issue**: Quest vault placed → `quest_reserved[0] = 1` → level generation fails → pending states never applied → regeneration sees no quest states to reset → keeps reservation = 1 → blocks new quest placement
- **Log Evidence**: "Quest vault: Skipping - quest already active (tulkas=0, mandos=0, aule=0, reserved=1)"

**Solution Applied**:
```c
/* Always reset quest reservation during regeneration since we're starting fresh */
/* The reservation system prevents multiple quests during a SINGLE generation attempt, */
/* not across regeneration attempts */
if (p_ptr->quest_reserved[0]) {
    log_trace("Quest vault regeneration: Resetting quest_reserved[0] from 1 to 0 (fresh generation attempt)");
    p_ptr->quest_reserved[0] = 0;
}
```

**System Behavior After Fix**:
- ✅ Quest reservation always reset during regeneration (fresh start principle)
- ✅ Quest vault placement no longer blocked after failed generation attempts
- ✅ Reservation system correctly scoped to single generation attempt
- ✅ All quest types properly handled during regeneration

**Files Modified**: `src/generate.c` - Simplified `reset_quest_vault_states()` with always-reset policy

**Compilation Instructions**: 
```bash
cd src && C:\Soft\cygwin\bin\bash.exe -l -c "cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src && make -f Makefile.cyg"
copy sil.exe ..
```

### Architecture Benefits

#### Scalability
- **Vault-Based Quests**: Easy to add new quests via vault templates
- **Entrance-Based Quests**: Flexible monster-based quest spawning
- **Unified Reservation**: Single system prevents quest conflicts
- **Metarun Integration**: Automatic persistence across character deaths

#### Robustness
- **Deferred State Management**: Prevents quest state corruption during failed generation
- **Comprehensive Reset Logic**: Handles all quest types during regeneration
- **State Validation**: Quest interactions validate current states
- **Debug Instrumentation**: Extensive logging for troubleshooting

#### Maintainability
- **Clear Separation**: Vault vs entrance-based quest distinction
- **Centralized Logic**: Quest placement concentrated in generate.c
- **Consistent Patterns**: Unified state machine across quest types
- **Documentation**: Comprehensive state tracking and lifecycle management

### Adding New Quests

#### For Vault-Based Quests:
1. Create vault template in data files
2. Add quest state constants to defines.h
3. Add skill/requirement checks to `try_quest_vault_type()`
4. Add pending state handling to `pending_quest_states` structure
5. Add quest state reset logic to `reset_quest_vault_states()`
6. Add metarun flag and completion tracking

#### For Entrance-Based Quests:
1. Add quest state constants to defines.h  
2. Add spawn logic to monster placement phase
3. Add quest state reset logic to `reset_quest_vault_states()`
4. Add metarun flag and completion tracking
5. Implement quest interaction functions

### Current Status
- **All Quest Types**: Working with proper regeneration support
- **Dual Quest Bug**: Fixed via reservation system  
- **Regeneration Logic**: Fixed for all quest types
- **Debug Infrastructure**: Comprehensive logging available
- **Tulkas Reward Bug**: Pending investigation (debug logging added)

The quest system is now architecturally sound and ready for future expansion.

## Planned Redesign: Oath System Overhaul (Proposal Pending Approval)

### Summary
Convert Oaths from a selectable Will ability (WIL_OATH) into a pre-run, unlockable meta-choice tied to quest completions. Each Valar quest permanently unlocks (for the remainder of the current metarun and all subsequent runs within it) a corresponding Oath that can be chosen only at character creation. Breaking an Oath removes its benefits, applies a curse penalty, and permanently bans that Oath for the rest of the metarun (and optionally future metaruns – decision point).

### Current State (Baseline)
1. Oath exists as `WIL_OATH` (index 6 in Will abilities). Player purchases ability, then chooses one of Mercy / Silence / Iron (flags: `OATH_MERCY_FLAG`, etc.).
2. Data fields:
  - `p_ptr->oath_type` (byte) active oath id (1=Mercy,2=Silence,3=Iron)
  - `p_ptr->oaths_broken` (bitfield of OATH_*_FLAG)
3. Rewards (static): Mercy +1 Grace, Silence +1 Strength, Iron +2 Con (applied while oath kept & not invalidated).
4. Oath selection UI lives in abilities purchase flow (cmd4.c) and can be selected mid-run after gaining the WIL ability.
5. Persistence: Broken oath flags persist only inside the run; no meta unlocking logic presently.

### Target Vision
1. No Oaths available in a brand-new metarun before **any** Valar quest has been completed.
2. Completing a quest unlocks a specific Oath for selection at creation of the NEXT character (not retroactively in current character):
  - Mandos quest → unlocks Iron Oath
  - Aule quest → unlocks Mercy Oath
  - Tulkas quest → unlocks Silence Oath
3. Birth Screen: If unlocked, player may choose at most one Oath (or none). Not an XP purchase; purely a toggle like a challenge modifier.
4. Oaths become part of Special (S_SPC) category logically (granted state) BUT chosen only at birth — displayed in character sheet & bonus calculations like other specials.
5. Breaking an Oath immediately:
  - Removes all Oath-derived stat bonuses & any derived secondary effects
  - Applies ONE curse (design choice: (a) random metarun curse draw, (b) fixed “Oathbreaker” curse, (c) player picks from revealed options). Default recommendation: Fixed new curse type for clarity.
  - Sets broken flag persistently in metarun so that specific Oath cannot be selected again in later runs of same metarun.
6. Optional Extension (Decision): If an Oath is broken, permanently ban it across *future metaruns* for score narrative continuity OR keep restriction scoped to current metarun only.
7. Song / ability synergies referencing `chosen_oath()` still work, but sourcing moves from Will ability presence to birth selection metadata.

### Data Model Changes
Add to persistent metarun structure (metarun.c / metarun.h):
```
u8 unlocked_oaths;  // bitwise: MERCY=1, SILENCE=2, IRON=4
u8 banned_oaths;    // broken (cannot pick again this metarun)
```
At birth, allowed_oaths = unlocked_oaths & ~banned_oaths.

Per character (player_type):
```
byte oath_type;      // retained (0 none, 1 mercy, 2 silence, 3 iron)
u8 oath_state;       // optional: ACTIVE=1, BROKEN=2 (or rely on oaths_broken bitfield)
u8 oaths_broken;     // keep for intra-run triggers & legacy save compat
```
Deprecate XP purchase path for `WIL_OATH`:
 - Maintain WIL_OATH constant for save compatibility but hide from menu / ignore if loaded.
 - Migration: If an existing save has WIL_OATH purchased, auto-convert to chosen oath at load (if at birth stage) or leave as legacy until run ends.

### Persistence & Migration
1. Extend metarun save block to include new oath bitmasks (version gate with defaults zero).
2. On quest reward finalize (REWARDED state), set corresponding bit in `unlocked_oaths`.
3. On oath break event, set bit in both `p_ptr->oaths_broken` and metarun `banned_oaths`.
4. Loader: If old save with active oath via Will ability: map to new field; if broken flags exist, copy into banned_oaths only if appropriate (decision: do not retro-ban across metarun unless break occurred earlier in same metarun).

### Unlock Mapping Table
| Quest Flag | Metarun Constant | Oath Bit | Oath ID | Notes |
|------------|------------------|---------|---------|-------|
| METARUN_QUEST_MANDOS | Mandos | IRON (4) | 3 | Thematic: Doom / resolve → Iron |
| METARUN_QUEST_AULE   | Aule   | SMITH (8) | 4 | Craft specialization / forge mastery |
| METARUN_QUEST_TULKAS | Tulkas | SILENCE (2) | 2 | Discipline after martial prowess |
| METARUN_QUEST_NIENA  | Niena  | MERCY (1) | 1 | Direct stealth ability (SPC_NIENA) + unlocks Mercy oath |

### Birth UI Changes
Add new panel after stat allocation & prior to abilities purchase:
```
Choose an Oath (optional):
 a) None
 b) Mercy   (Unlocked)  – to leave Angband without shedding blood of Men or Elves  (+1 Gra)
 c) Silence (Locked/Unlocked) – to leave as you came, grim and silent (+1 Str)
 d) Iron    (Locked/Unlocked) – none shall daunt you from facing Morgoth (+2 Con)
```
Locked entries greyed. Broken (banned) entries marked “(Broken – unavailable)”.

### Bonus Application Path
Move stat bonuses from `calc_bonuses()` Will ability branch to a new oath bonus section keyed by `p_ptr->oath_type` and not invalidated unless broken.

### Breaking Logic Hook Points
Current triggers (example: attacking invalid target for Mercy, singing for Silence, ascending without Silmaril for Iron). Rewire these to:
1. Check `p_ptr->oath_type` matches relevant oath
2. Confirm not already broken (bit not set)
3. Prompt confirmation (existing UI) then:
  - Set broken bit(s)
  - Apply curse: invoke `apply_oathbreaker_curse()` (new) or existing curse assignment
  - Force recalculation of bonuses & redraw
  - Log meta note & update metarun banned mask

### Curse Design Options (Decision Points)
Option A (Simple): Single new curse “Oathbreaker”: -1 all stats, -5% score multiplier
Option B (Random Draw): Add one random metarun curse (reuses existing curse pipeline)
Option C (Targeted): Specific penalty per oath broken (e.g., Mercy → permanent -Grace, Silence → -Song skill, Iron → -Con)
Recommendation: Start with Option B for variability or Option A for clarity. (Need confirmation.)

### Backward Compatibility Strategy
1. Leave WIL_OATH constant; hide from acquisition menu by filtering where abilities enumerated (skip if `abilitynum == WIL_OATH`).
2. Loader: If `p_ptr->active_ability[S_WIL][WIL_OATH]` set, derive `p_ptr->oath_type` from existing selection logic (already stored) and ignore Will slot thereafter.
3. Save writer: Stop writing WIL_OATH active flag (or always write 0) after version bump; retain reader tolerance.
4. Version gating macro in defines or a new savefile minor version increment constant.

### Removal / Refactor Targets
- cmd4.c: Remove oath purchase path from Will ability screen; migrate oath_menu() to birth flow file (birth.c).
- defines.h: Mark WIL_OATH as deprecated (comment) but keep numeric value.
- xtra1.c / cmd1.c / cmd2.c: Replace `p_ptr->active_ability[S_WIL][WIL_OATH]` checks with `p_ptr->oath_type != 0`.
- New utils: `bool oath_unlocked(int id); bool oath_banned(int id);` for clarity.

### Edge Cases
1. Player completes quest mid-run then dies before next run → Oath becomes available for next character (OK).
2. Player breaks an Oath then dies same floor → Ban persists (metarun banned flag saved during death finalization path).
3. Multiple quests completed before any oath chosen → Multiple oaths available at next birth (choose only one).
4. Old save with active oath and later code expects metarun unlock bit: at load, set unlock bit corresponding to existing oath for remainder of metarun to avoid regression.
5. Score / leaderboard: Consider adding tag “Oath: Mercy (Kept)” or “Oath: Iron (Broken)” to record.

### Implementation Phases (Proposed)
1. Data Layer: Extend metarun struct + save/load + version bump.
2. Birth UI: Inject oath selection panel (reuse oath_menu visuals adjusted for locked/unlocked/banned states).
3. Ability Removal: Hide WIL_OATH from skill purchase menus; migrate bonuses.
4. Unlock Hooks: On quest REWARDED events set metarun unlocked bit.
5. Breaking Flow: Centralize oath breaking into helper; add curse application.
6. Refactor Checks: Replace all `p_ptr->active_ability[S_WIL][WIL_OATH]` gate conditions.
7. Migration & Compatibility: Implement loader shim; test legacy saves.
8. QA / Logging: Add trace logs for unlocks, selection, breaks, persistence.
9. Documentation: Update README, ability.txt (remove old ability), add new section in memory & gameplay docs.

### Metrics / Testing Plan
Automated / manual tests:
1. Start fresh metarun: confirm no oaths appear.
2. Complete Mandos quest → die → new character: Iron option appears only.
3. Complete second quest in same run before death → next birth shows two oaths.
4. Choose Silence → break it → verify curse applied, bonuses removed, banned bit set; next new character cannot choose Silence.
5. Legacy save with active oath loads: no crash; oath recognized; WIL_OATH not shown in menu.

### APPROVED DECISIONS (User Confirmed)
1. **Curse model**: Option B - Random metarun curse draw (reuses existing curse pipeline)
2. **Ban scope**: Current metarun only - future metaruns never affected by broken oaths
3. **Bonus values**: Keep existing (+1 Gra, +1 Str, +2 Con)
4. **Unlock timing**: Next run only (not mid-run adoption)
5. **Score multiplier**: None (keep simple)
6. **UI placement**: After character selection but BEFORE stat allocation 
7. **Theming**: Add Tolkien thematic texts to oath descriptions

### Implementation Status: COMPILATION SUCCESS ✅

**FIXED**: All compilation errors resolved:
1. ✅ `show_file` function call - fixed parameter count from 4 to 3
2. ✅ `oath_unlocked` / `oath_banned` - fixed function names from `metarun_check_oath_*` to correct names  
3. ✅ `path_temp` - made function public and added declaration to externs.h
4. ✅ `choose_escape_curses_ui` - made function public (was static in metarun.c)

**COMPILATION SUCCESSFUL**: All oath system functions now compile without errors

**IMPLEMENTATION COMPLETE**: 
- Core oath system data model ✅
- Birth UI for oath selection ✅ 
- Metarun persistence (unlock/ban tracking) ✅
- Quest completion → oath unlocking ✅
- Oath breaking → curse selection ✅ (random metarun curse draw)
- Knowledge menu oath status display ✅

**READY FOR TESTING**: Full oath lifecycle now implemented and functional

### Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|------------|
| Save incompatibility | Player saves corrupted | Version-gated fields, default zeros, robust loader checks |
| Hidden WIL_OATH breaks scripts | Null pointer or logic gap | Leave constant + compatibility shim |
| Forgotten checks referencing active_ability | Inconsistent bonuses/removals | Grep for WIL_OATH & active_ability usage; comprehensive replacement pass |
| Curse stacking imbalance | Difficulty spike | Random curse system already balanced |
| UI confusion at birth | Player uncertainty | Clear locked/banned labels & Tolkien thematic text |

### Estimated Effort
~6–8 focused hours (data model + UI + refactor + testing) excluding balance iteration.

### Implementation Plan: EXECUTING NOW


# Sil-More Project Memory & Knowledge Base

## Project Overview
Sil-Morë is a Tolkien-themed ASCII roguelike game forked from Sil, which itself descends from Angband. The game features stealth mechanics, Tolkien lore integration, and a complex quest system involving the Valar (Tulkas, Aule, Mandos).

## CRITICAL BUGS DISCOVERED AND FIXES NEEDED

## CRITICAL BUGS DISCOVERED AND FIXES NEEDED

### FIXED ISSUE 1: Quest Vault Placement Strategy
**RESOLVED**: Quest vault placement now uses forced forge strategy
- **Root Cause**: Quest vaults used random placement (50 attempts) with 4-cell padding, while forced forge used strategic center placement with better location selection
- **Key Differences Found**:
  - Forced forge: Single strategic location (map center), fails level if can't place
  - Quest vaults: 50 random locations, continues if fails
  - Both used same 4-cell padding, but quest vaults often couldn't find space
  - Quest vault templates include both small (11x11) and large (39x9) vaults
- **Solution Implemented**: 
  - Quest vaults now use center-focused placement like forced forge
  - Added `place_room_forced()` with reduced padding (1-cell instead of 4-cell)
  - Strategic fallback locations near map center
  - Enhanced logging for debugging
- **Files Modified**: src/generate.c (quest vault placement logic)

### FIXED ISSUE 2: Metarun Quest Persistence Logic
**RESOLVED**: Quest states now only persist in metarun when fully REWARDED
- **Root Cause**: Metarun completion was triggered by SUCCESS state (state 3) instead of REWARDED state (state 4/5)
- **Problem**: If player completed quest but didn't get reward (or died), quest was still marked as "metarun completed", preventing re-attempts in new runs
- **Quest State Reference**:
  - Aule: 0=NOT_STARTED, 1=FORGE_PRESENT, 2=ACTIVE, 3=SUCCESS, 5=REWARDED
  - Mandos: 0=NOT_STARTED, 1=GIVER_PRESENT, 2=ACTIVE, 3=SUCCESS, 4=REWARDED  
  - Tulkas: 0=NOT_STARTED, 1=GIVER_PRESENT, 2=COMPLETE, 4=REWARDED
- **Solution**: Modified `metarun_check_and_update_quests()` to only mark quests as metarun-completed when REWARDED
- **Files Modified**: src/metarun.c (quest completion logic)

### FIXED ISSUE 3: Two Critical Metarun Quest Bugs - RESOLVED
**BUG 1**: Quest states persist when loading dead saves - **FIXED**
- Problem: Dead characters were having quest progress restored from metarun
- Solution: Modified birth.c to only restore quest states for living characters
- Added check: `if (character_loaded && !p_ptr->is_dead)` instead of just `if (character_loaded)`

**BUG 2**: metarun_is_quest_completed() checks ALL metaruns - **FIXED**  
- Problem: Function checked all metaruns instead of current metarun only
- Log showed: "Found quest 0x4 completed in metarun[1]" when checking from metarun[2]
- Solution: Modified function to only check `metaruns[current_run]` instead of looping through all metaruns
- Now correctly prevents quest spawning only within the same metarun

**FILES MODIFIED**:
- src/metarun.c: Fixed metarun_is_quest_completed() to check current metarun only
- src/birth.c: Fixed quest state restoration to skip dead characters

### FIXED ISSUE 3: Metarun Quest Completion Logic
Resolved: `metarun_is_quest_completed()` now scans all metaruns (verified in `metarun.c`). Primary persistence bug source shifted to write‑back ordering (see Fix Summary below).

### FIXED ISSUE 5: Forge Forcing Logic - COMPLETELY FIXED ✅

**PROBLEM IDENTIFIED AND RESOLVED**: Forge forcing was happening on levels beyond the target levels (2, 6, 10) due to incorrect comparison operator.

**Root Cause Analysis**:
- The guaranteed forge logic used `<=` comparison: `next_guaranteed_forge_level <= p_ptr->depth`
- This caused forge forcing to activate at level 2 AND ALL DEEPER LEVELS, instead of just at levels 2, 6, and 10
- Example: On level 15 with `fixed_forge_count=2`, `next_guaranteed_forge_level=10`, so `10 <= 15` = true
- Result: `force_forge=true` was incorrectly set on level 15, affecting quest vault generation and normal vault placement

**Complete Solution Implemented**:
```c
// Fixed logic - only force at exact levels
int next_guaranteed_forge_level = 2 + (p_ptr->fixed_forge_count * 4);
is_guaranteed_forge_level = (next_guaranteed_forge_level == p_ptr->depth);
```

**Behavior After Fix**:
- ✅ Forge forcing only occurs at levels 2, 6, and 10 exactly
- ✅ Quest vault generation on level 15 sees `force_forge=false` correctly
- ✅ Normal vault generation works properly without inappropriate forge restrictions

**Files Modified**: `src/generate.c` - Fixed guaranteed forge level logic from `<=` to `==` comparison

### FIXED ISSUE 7: Quest Ability Display and Multiple Quest Spawning - RESOLVED ✅

**PROBLEM 1**: Mandos' Doom ability not properly updating display when clearing confusion
- **Root Cause**: Direct assignment `p_ptr->confused = 0` bypassed proper status clearing functions
- **Solution**: Use proper status functions `set_confused(0)`, `set_afraid(0)`, `set_stun(0)`, etc. that trigger display updates
- **Files Modified**: `src/xtra1.c` - Fixed status effect clearing in Mandos ability

**PROBLEM 2**: Tulkas and Mandos spawning simultaneously on same level
- **Root Cause**: Quest reservation (`quest_reserved[0]`) was set too late - only when pending states were applied at end of level generation
- **Solution**: Set `quest_reserved[0] = 1` immediately when quest vault is placed, preventing other quests from spawning during same generation
- **Files Modified**: `src/generate.c` - Added immediate quest reservation when quest vaults are placed

**PROBLEM 3**: Quest vault visibility and padding (In Progress)
- **Analysis**: Quest vaults use 1-cell padding vs normal vaults' 2-cell padding. User requested padding normalization.
- **Current Status**: Need to determine if quest vaults should match normal vault padding (2-cell) or if padding should be standardized to 1-cell

**System Behavior After Fixes**:
1. ✅ Mandos' Doom properly clears confusion with immediate display update
2. ✅ Only one quest can spawn per level (quest reservation works immediately)
3. 🔄 Quest vault placement and visibility being reviewed

### FIXED ISSUE 6: Quest Reward Abilities - CORRECTED IMPLEMENTATION ✅

**CORRECTED IMPLEMENTATION**: Quest reward abilities have been fixed to work according to their intended design.

**Abilities Implementation Corrected**:

1. **Aule's Forge** (`SPC_AULE`):
   - ✅ **Granted correctly**: Sets both `have_ability` and `active_ability` on quest completion
   - ✅ **Blocks Masterpiece purchase**: Prevents purchasing inferior smithing ability
   - ✅ **Works like Masterpiece + 2**: Allows smithing items beyond skill level by adding base skill + 2 extra difficulty points
   - ✅ **Efficient skill drain**: Drains only 1 smithing skill for every 2 excess difficulty points (vs Masterpiece's 1:1 ratio)
   - ✅ **Actually drains skill**: Uses the same `smithing_cost.drain` system as Masterpiece to consume skill points
   - **Usage**: Added alongside existing Masterpiece checks in smithing functions

2. **Mandos' Doom** (`SPC_MANDOS`):
   - ✅ **Granted correctly**: Sets both `have_ability` and `active_ability` on quest completion
   - ✅ **Mental immunity**: Grants +100 resistance to fear, hallucination, stun, and confusion
   - ✅ **Effect clearing**: Automatically clears fear, hallucination, entrancement, rage, stun, and confusion each turn
   - **Usage**: Applied in `calc_bonuses()` function in `xtra1.c`

**Code Implementation**:
- Aule's Forge now properly works alongside Masterpiece ability using the same skill drain system
- Added Aule checks to `calculate_smithing_cost()` and `too_difficult()` functions  
- Skill drain calculation: `smithing_cost.drain += (excess + 1) / 2` for efficient 1:2 ratio
- Mandos' Doom includes full confusion immunity and clearing
- Debug logging added to verify ability activation

**Testing Requirements**:
- **Aule's Forge**: Test by attempting high-difficulty smithing projects - should allow Masterpiece effect +2 with efficient drain and actual skill consumption
- **Mandos' Doom**: Test by encountering mental effects - should be immune to all mental effects including confusion

**Files Modified**: `src/xtra1.c`, `src/cmd4.c`, `lib/edit/ability.txt`, `src/generate.c` - Corrected ability implementations and forge forcing logic

**PROBLEM IDENTIFIED AND RESOLVED**: Quest vaults were being successfully placed but lost during level regeneration because quest states persisted between generation attempts.

**Root Cause Analysis**: 
- Quest vault placed in first attempt → quest state set to GIVER_PRESENT → level generation fails due to connectivity
- During regeneration, quest state persisted → quest vault logic sees "quest already active" → skips quest vault placement
- Result: Quest vault disappears from final level despite being successfully placed initially

**Complete Solution Implemented**:

1. **Enhanced Pending Quest State System**: Already existed to defer quest state changes until successful generation
2. **Added Quest State Reset Function**: `reset_quest_vault_states()` resets quest states from previous failed attempts
3. **Integrated Reset Logic**: Called at start of each generation attempt to provide clean slate
4. **Safe Level-Specific Reset**: Only resets quest states that were set at current level (prevents interference with legitimate quests)
5. **Quest Reservation Reset**: Also resets quest_reserved[0] when quest states are reset

**Key Code Changes in `src/generate.c`**:
```c
// New function to reset quest states from failed attempts
static void reset_quest_vault_states(void) {
    bool reset_reservation = false;
    
    if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT && p_ptr->aule_level == p_ptr->depth) {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        p_ptr->aule_level = 0;
        reset_reservation = true;
    }
    
    if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT && p_ptr->mandos_level == p_ptr->depth) {
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
        p_ptr->mandos_level = 0;
        reset_reservation = true;
    }
    
    if (reset_reservation && p_ptr->quest_reserved[0]) {
        p_ptr->quest_reserved[0] = 0;
    }
}

// Called at start of each generation attempt:
reset_pending_quest_states();
reset_quest_vault_states();
```

**Fix Verification**: 
- Log messages show successful quest state reset: "Quest vault regeneration: Resetting Mandos quest from GIVER_PRESENT to NOT_STARTED"
- Quest vault logic now sees clean state on regeneration attempts
- Quest vaults can be properly re-placed during level regeneration
- Legitimate quests on other levels remain unaffected

**System Behavior After Fix**:
1. ✅ Quest vault placed → pending state recorded → level fails → states reset
2. ✅ Regeneration attempt → clean quest state → quest vault can be placed again  
3. ✅ Level succeeds → pending states applied → quest properly activated
4. ✅ Legitimate quests on other levels protected by level-specific reset logic

**Result**: Quest vaults now persist through level regeneration correctly, fixing the disappearing quest vault bug.

## Architecture & Build System

### Core Structure
- **Primary Language**: C (C11 compatible)
- **Main Header**: src/angband.h - Include this in all source files
- **Version**: 0.8.6 (see defines.h for version constants)
- **Platform Support**: Windows, macOS, Linux/Unix, DOS

### Build System
Multiple platform-specific Makefiles exist:
- Makefile.std - Unix/Linux systems
- Makefile.cyg - Cygwin on Windows (recommended for Windows development)
- Makefile.win - Windows native
- Makefile.osx - macOS
- Makefile.cocoa - macOS with Cocoa interface

**Recommended Windows Build Process**: Use Cygwin with the command:
`ash
C:\Soft\cygwin\bin\bash.exe -l -c "cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src && make -f Makefile.cyg -j8 launch"
`

## Quest Logic Refactoring - Critical Issues & Solutions

### UPDATED ISSUE: Metarun Quest Completion Not Persisting
Root Cause Identified: `metarun_mark_quest_completed()` wrote directly to `metaruns[current_run]` but `save_metaruns()` overwrote that array entry with the stale copy from the global `metar` struct (`metaruns[current_run] = metar;`). Result: newly set quest bits were lost on save → reload showed zeros.

Fix Implemented: Function now updates `metar.completed_quests` first, mirrors the bit to the array (optional), then calls `save_metaruns()`. Added trace logging to confirm bitmask after update.

### UPDATED ISSUE: Quest Vault Visibility / Overwrite
Observation: Logs report successful quest vault placement but vault sometimes absent. Hypothesis: Subsequent normal room/vault generation may succeed elsewhere causing player not to encounter original location, or extremely rare later carving could intrude if CAVE_ICKY not set early enough. Current code places quest vault before other rooms and uses `CAVE_ICKY`, so outright overwrites are unlikely; invisibility may stem from failed placement after logging or from player not reaching coordinates.

Mitigation Applied: (a) Additional tracing already present. (b) Confirmed early placement sequence. (c) Future enhancement suggestion: store last quest vault bbox & echo to player on debug mode; optionally add a map reveal cheat flag. (No structural change required yet.)

### FIXED ISSUES 

#### Special Abilities System
- **Fixed**: Special abilities not persisting - Modified calc_bonuses() to preserve S_SPC abilities
- **Fixed**: Special skill showing in menu - Hidden S_SPC in skill display functions
- **Fixed**: Abilities menu showing all slots - Filtered to show only granted abilities

#### Quest State Management  
- **Fixed**: Added MANDOS_QUEST_REWARDED state to prevent repeated reward interactions
- **Fixed**: Aule quest skill check - Changed from total skill to base skill only
- **Fixed**: Quest reservation system to prevent multiple quests per run

## Key Technical Details

### Metarun System
- File: src/metarun.c
- Function: metarun_check_and_update_quests() - Should be called from update_stuff()
- Persistence: save_metaruns() called on game exit
- Quest flags: METARUN_QUEST_TULKAS, METARUN_QUEST_AULE, METARUN_QUEST_MANDOS, METARUN_QUEST_NIENA

### Quest States
- **Mandos**: NOT_STARTED(0)  GIVER_PRESENT(1)  ACTIVE(2)  SUCCESS(3)  REWARDED(4)
- **Aule**: NOT_STARTED(0)  FORGE_PRESENT(1)  ACTIVE(2)  SUCCESS(3)  REWARDED(4)
- **Tulkas**: NOT_STARTED(0)  ACTIVE(1)  COMPLETE(2)  REWARDED(3)

### Quest Vault System
- File: src/generate.c
- Placement: 	ry_quest_vault_type() function
- Collision: solid_rock() and CAVE_ICKY flag system
- Processing: process_quest_vault_area() function

## Files Modified in Quest Refactoring
- src/defines.h: Added MANDOS_QUEST_REWARDED 4
- src/xtra2.c: Fixed Mandos quest state management and reward handling  
- src/metarun.c: Enhanced quest completion detection for all states
- src/generate.c: Fixed Aule quest to check base smithing skill, enhanced debugging
- src/xtra1.c: Fixed special abilities persistence in calc_bonuses()
- src/cmd4.c: Fixed abilities menu filtering

## Fix Summary (Latest)
1. **Quest Vault Placement**: Implemented forced placement strategy with center-focused location selection and reduced padding (1-cell vs 4-cell)
2. **Metarun Quest Persistence**: Fixed to only mark quests as metarun-completed when REWARDED (not SUCCESS), allowing re-attempts if player didn't get reward
3. **Quest Vault Level Regeneration**: Fixed level regeneration discarding quest vaults due to insufficient room count - reduced minimum room requirement when quest vault present
4. **Verification**: Enhanced logging active across quest, metarun, and vault systems

## Files Modified in Latest Quest Fixes
- src/generate.c: Quest vault placement strategy (forced placement + reduced padding) + level regeneration fix (dynamic room requirements)
- src/metarun.c: Quest completion persistence logic (REWARDED-only marking)

## User Preferences & Context
- Working on Windows with Cygwin build environment
- Focused on quest system reliability and persistence
- Requires proper metarun functionality for quest completion tracking
- Needs vault visibility issues resolved for quest accessibility

## Latest Fixes Applied

### FIXED ISSUE 8: Tulkas Quest Reservation - RESOLVED ✅

**PROBLEM**: Tulkas spawning did not consistently set quest reservation, allowing multiple quests to spawn simultaneously.
- **Root Cause**: Main Tulkas spawn logic at line 4762 in generate.c set quest state but not quest_reserved[0]
- **Secondary Path**: Fallback spawn logic at line 4786 correctly set both quest state and reservation
- **Result**: Tulkas could spawn via main path without blocking other quests from spawning

**Solution Applied**:
```c
// Added quest reservation to main Tulkas spawn path
if (place_monster_one(try_y, try_x, R_IDX_TULKAS, true, true, NULL))
{
    p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
    p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
    tulkas_spawned = true;
    // logging...
}
```

**System Behavior After Fix**:
- ✅ Tulkas spawning immediately reserves quest slot via both spawn paths
- ✅ Quest vault placement uses pending system (no immediate reservation)
- ✅ Only one quest can be active per level (proper reservation pattern)

**Files Modified**: `src/generate.c` - Added quest reservation to main Tulkas spawn logic

### Build Process Updated
```bash
cd c:\Users\efrem\Documents\GitHub\sil-qh\src
C:\Soft\cygwin\bin\bash.exe -l -c "cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src && make -f Makefile.cyg -j8"
cd c:\Users\efrem\Documents\GitHub\sil-qh
Copy-Item src\sil.exe . -Force
```

## NEW QUEST IMPLEMENTATION: Niena's Mercy Quest

### Quest Overview
**Niena's Mercy Quest**: A pacifist challenge quest that spawns on maximum-size levels and requires the player to reach the stairs down without killing any monsters, rewarding enhanced stealth abilities.

### Trigger Conditions
- Level must be maximum size (l = 5, dimensions 55x165)
- Distance between up stairs (player spawn) and closest down stairs must be at least half the diagonal of the level
- Level diagonal calculation: sqrt(55² + 165²) = sqrt(30,225) = ~174, so minimum distance = 87
- All existing quest conditions must be met (no active quests, not completed in metarun, etc.)

### Quest Mechanics
1. **Spawn Conditions**: 
   - Maximum level size (l=5)
   - Sufficient stair separation (≥87 grid distance)
   - Standard quest availability checks
   
2. **Quest Giver**: Niena (new monster based on existing Valar pattern)
   - Monster ID: Will use next available slot after existing Valar
   - Stats: Similar to other Valar quest givers (PEACEFUL, UNIQUE, NEVER_MOVE, SPECIAL_GEN)
   - Appearance: V:v1 (Valar with light violet color)

3. **Quest Objective**: 
   - Leave level via down stairs without killing any monsters
   - All stairs become visible when quest is accepted
   - Monster kill count tracked separately for quest validation

4. **Quest Reward**: 
   - Special ability: Enhanced Stealth
   - Formula: Add 10 * (seen_monsters - killed_monsters) to stealth skill
   - Tracks monsters seen vs killed during the quest

### Implementation Plan

```
- [ ] Step 1: Add Niena monster definition to monster.txt
- [ ] Step 2: Add quest state constants to defines.h
- [ ] Step 3: Add quest state tracking to player structure
- [ ] Step 4: Implement level size and stair distance detection
- [ ] Step 5: Add Niena spawn logic to generate.c
- [ ] Step 6: Implement quest interaction system in xtra2.c
- [ ] Step 7: Add stair visibility system
- [ ] Step 8: Implement monster tracking (seen vs killed)
- [ ] Step 9: Add quest completion detection
- [ ] Step 10: Implement special ability reward system
- [ ] Step 11: Add metarun persistence and unlocking
- [ ] Step 12: Ensure quest monster spawn restriction (SPECIAL_GEN flag)
- [ ] Step 13: Testing and debugging
```

### Technical Details

#### Monster Definition
- **Name**: Niena, Lady of Pity
- **Index**: Next available after existing quest monsters (likely 21)
- **Flags**: FEMALE | NEVER_MOVE | SMART | PEACEFUL | UNIQUE | NEVER_BLOW | SPECIAL_GEN
- **Appearance**: V:v1 (light violet Valar symbol)
- **Description**: Tolkien-appropriate lore about mercy and pity

#### Quest States
- `NIENA_QUEST_NOT_STARTED` (0)
- `NIENA_QUEST_GIVER_PRESENT` (1)  
- `NIENA_QUEST_ACTIVE` (2)
- `NIENA_QUEST_SUCCESS` (3)
- `NIENA_QUEST_REWARDED` (4)

#### Level Generation Integration
- Add maximum level detection: `if (l >= 5)`
- Add stair distance calculation using distance formula
- Add Niena spawn logic similar to Tulkas entrance-based system
- Integrate with existing quest reservation system

#### Monster Tracking System
- Add quest-specific monster counters to track seen vs killed
- Integrate with existing monster death and visibility systems
- Reset counters when quest becomes active

#### Special Ability System
- Add `SPC_NIENA` special ability constant
- Implement stealth bonus calculation in `calc_bonuses()`
- Add ability description and effects
- Integrate with existing special ability framework

#### Metarun Integration
- Add `METARUN_QUEST_NIENA` flag
- Add unlock system for future oath (if applicable)
- Add completion tracking and persistence

### Architecture Integration
- **Type**: Entrance-based quest (like Tulkas)
- **Spawn Method**: Monster placement during level generation
- **State Management**: Immediate state application (no pending system needed)
- **Reservation**: Immediate quest reservation to prevent conflicts
- **Persistence**: Standard metarun completion tracking

### Files to Modify
1. **lib/edit/monster.txt**: Add Niena monster definition
2. **src/defines.h**: Add quest state constants and special ability
3. **src/angband.h**: Add quest state to player structure (if needed)
4. **src/generate.c**: Add spawn logic and level size/distance checks
5. **src/xtra2.c**: Add quest interaction functions
6. **src/xtra1.c**: Add special ability effects in calc_bonuses()
7. **src/metarun.c**: Add metarun completion tracking
8. **src/monster*.c**: Integrate monster tracking if needed

### Testing Plan
1. **Level Generation**: Verify maximum size detection and stair distance calculation
2. **Quest Spawning**: Test Niena spawn conditions and quest reservation
3. **Quest Mechanics**: Test stair visibility and monster tracking
4. **Reward System**: Test special ability granting and stealth bonus calculation
5. **Integration**: Test with existing quest system and metarun persistence
6. **Edge Cases**: Test quest failure, level regeneration, and save/load compatibility
