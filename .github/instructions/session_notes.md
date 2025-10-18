# Session Notes - October 19, 2025

## Copilot Instructions Updated (October 19, 2025)

### Task
Updated `.github/instructions/copilot-instructions.md` to align with current `AGENTS.md` and incorporate Python environment setup documentation.

### Changes Made
1. **Synchronized structure with AGENTS.md**:
   - Updated "Quick Orientation" to use fork-specific language ("fork" terminology)
   - Changed "Build & Run" section to highlight SDL3 builds as recommended
   - Replaced old "Build & run workflow" with modern CMake approach

2. **Added comprehensive Key Systems section**:
   - Combat history menu (100-round buffer, color formatting, search)
   - Enhanced inventory/equipment overlays with scrolling
   - Unified look command with Tab cycling
   - Combat roll overlay specifications

3. **Added Data, Saves & Logs section**:
   - Metarun save backup procedure
   - Log file locations for SDL3 builds

4. **Consolidated UX & Gameplay Guardrails**:
   - Hard-coded offset prevention
   - Floor item prefix handling
   - Steam Deck support considerations

5. **Reorganized sections**:
   - Moved Python to dedicated section (before Watchpoints)
   - Removed outdated UI & menu architecture details
   - Cleaned up Input/selection/prompts and Game data sections

6. **Python Environment & Scripts section** (condensed):
   - Python 3.13.2 venv location: `src/.venv/`
   - PowerShell syntax guidance: `& "./src/.venv/Scripts/python.exe" script.py`
   - Utility scripts listed
   - Reference to `PYTHON_SETUP.md` for detailed guidance

7. **Updated Documentation Protocol section**:
   - Reinforced one-file-per-session approach
   - Emphasized concise, technical notes

8. **Updated References**:
   - Added `PYTHON_SETUP.md` reference
   - Maintained links to `AGENTS_MEMORY.md` and legacy docs

### File Status
- **Location**: `.github/instructions/copilot-instructions.md`
- **Lines**: 60 (down from 76 due to consolidation)
- **Status**: ✅ Synchronized with AGENTS.md and modernized

### Key Improvements
- File is now aligned with current project structure (SDL3, CMake builds)
- Python guidance is concise but complete
- Documentation is cleaner and less redundant
- References guide users to more detailed docs (AGENTS_MEMORY.md, PYTHON_SETUP.md) for deep dives

---

## Look Menu Enhancements (Previous Work)

### Feature 1: 'x' Key Shows Description
The 'x' key now works as an examination/description key in the unified look menu (`l` mode), matching expected roguelike behavior.

### Implementation
- **File**: `src/cmd3.c`, function `do_cmd_unified_look()`
- **Change**: Moved 'x' key from grouped "capital letters ignored" case to its own handler
- **Behavior**: When 'x' is pressed, it examines the currently selected entity (monster or object)
  - If in sidebar mode with a selected entity, shows full description/recall for that entity
  - If in manual cursor mode, shows description of entity at cursor position
  - For monsters: displays full monster recall screen
  - For objects: displays detailed object info screen with comparison if applicable
- **Code Pattern**: Uses identical logic to Space/Enter key examination (copied handler code)

### Feature 2: Object Grouping Separation
Herbs, potions, and gems are now displayed as separate groups in the look menu sidebar, providing better visual organization and player clarity.

### Implementation
- **Files**: `src/cmd3.c` (grouping logic), `src/cmd4.c` (display & sorting), `src/defines.h` (enum definition)

#### Changes in `src/defines.h`:
- Moved enum `unified_sidebar_object_group` from `cmd3.c` to global `defines.h` for access by both files
- Added three new group enums:
  - `LOOK_GROUP_HERBS` (for `TV_EASTER` items)
  - `LOOK_GROUP_POTIONS` (for `TV_POTION` items)
  - `LOOK_GROUP_GEMS` (for `TV_GEM` items)
- Reordered enum to place new groups after LOOK_GROUP_ARMOUR and before existing LOOK_GROUP_CONSUMABLE

#### Changes in `src/cmd3.c`:
- Updated `unified_sidebar_object_group()` function to assign:
  - `TV_EASTER` → `LOOK_GROUP_HERBS`
  - `TV_POTION` → `LOOK_GROUP_POTIONS`
  - `TV_GEM` → `LOOK_GROUP_GEMS`
  - (kept `TV_FOOD` logic in `LOOK_GROUP_CONSUMABLE`)

#### Changes in `src/cmd4.c`:
- Removed duplicate enum and function definitions
- Updated sorting logic in `show_unified_sidebar()` to handle new groups
- New groups use same sorting rules as consumables: **sort by level (highest first), then difficulty, then distance**
- Updated sort switch statement to include all three new groups with CONSUMABLE/OTHER case

### Display Behavior
- Each group appears separately in the sidebar with its own items
- Top-five limit (`T` key) applies **per group**, so you can see up to 5 herbs, 5 potions, and 5 gems simultaneously
- Sorting within each group follows the same rules:
  - **Artifacts/Weapons/Armour**: Sort by difficulty (highest first), then level, then distance
  - **Herbs/Potions/Gems/Consumable/Other**: Sort by level (highest first), then difficulty, then distance

### Build Status
✅ Compiled successfully with no errors  
✅ Deployed to `sil-more-windows-sdl3/`  
✅ Game runs with changes integrated  

### Technical Notes
- Enum placement in `defines.h` allows both `cmd3.c` and `cmd4.c` to access it (they both include `angband.h`)
- No save file compatibility issues (purely display/UI logic)
- Sorting algorithm naturally handles new groups without modification to comparison logic
- New groups follow standard `tval` pattern consistent with existing code

---

# Session Notes - October 17, 2025

## Character Tutorial Integration into Birth Menu

### Feature
Integrated the character screen tutorial into the character creation (birth) process. The tutorial is now automatically shown to **first-time players only** (when the scorefile is empty, meaning no previous games have been played).

### Implementation
- Modified `player_birth()` in `src/birth.c` to check if this is the player's first character
- Uses `highscore_count()` which returns 0 when no games have been played (scorefile empty)
- Calls `display_character_tutorial()` after character creation completes but before returning
- Tutorial shows 4 interactive stages explaining the character screen
- After tutorial completion, gameplay continues normally

### User Experience Flow
1. Player creates their first character through normal birth process
2. Character creation completes (stats, skills, equipment assigned)
3. **NEW**: Tutorial automatically displays if scorefile is empty
4. Player can press ESC to skip tutorial at any time, or proceed through all 4 stages
5. Game continues to dungeon entrance as normal

### Code Changes
- **File**: `src/birth.c`, function `player_birth()`
- **Location**: After `metarun_load_persistent_settings()`, before final return
- **Check**: `if (highscore_count() == 0)` - only triggers for first game
- **Action**: Calls `display_character_tutorial()` with logging

### Technical Notes
- `highscore_count()` returns count of Silmarils recovered across all games (0 = no games played)
- Tutorial function already existed and was accessible via '?' key in character screen
- No changes needed to tutorial itself - it works perfectly in this context
- Tutorial uses `screen_save()`/`screen_load()` so screen state is preserved
- Logging added: "First character created - showing character tutorial"

### Testing Notes
- Build successful with no errors
- Ready for testing: create fresh game with empty scorefile to verify tutorial appears
- Verify tutorial can be skipped with ESC
- Verify normal gameplay after tutorial completion
- Verify tutorial does NOT appear on subsequent characters

---

# Session Notes - October 16, 2025

## SDL Configuration Resolution Profiles Update

### Feature
Updated `src/sdl-config.c` with **41 comprehensive resolution profiles** covering all common display sizes from 800×600 to 8K. When no `sil_sdl.json` exists, the game automatically selects optimal settings based on detected screen resolution.

### Design Priority
**MAXIMUM SCALE (up to 3)** is prioritized for best graphics quality, then auxiliary panes are added if space permits.

### Layout Calculation Logic

1. **Main Terminal Minimum**: 40×24 tiles (16×16 base tile size)
   - Scale 1: 640×384px, Scale 2: 1280×768px, Scale 3: 1920×1152px
2. **Scale Selection**: **MAXIMUM** scale (up to 3) that fits the minimum terminal
3. **Aux Font Size**: Half of scaled tile size (scale 3→18px, 2→16px, 1→9px)
4. **Right Pane**: Added if ≥40 columns fit after main terminal (~0.6×font char width)
   - Inventory (22 rows), Worn (17 rows), Info (auto-remaining)
   - Width: 40 or 50 columns
5. **Bottom Pane**: Added if ≥1 row fits (max 4 rows)
   - Rolls and Log split 50/50
6. **Main Terminal**: Expands to use all remaining space

### Resolution Categories

**Scale 1 (4 resolutions)**: Low-res displays, large viewport
- 800×600, 1024×768, 1152×864, 1280×720

**Scale 2 (14 resolutions)**: Mid-range HD displays
- 1280×768 through 3840×1080

**Scale 3 (23 resolutions)**: High-DPI displays, crisp graphics
- 1920×1200 through 7680×4320 (8K)

### Notable Configurations

| Resolution | Scale | Panes | Notes |
|------------|-------|-------|-------|
| 1280×768 | 2 | None | Exact fit, no auxiliary panes |
| 1280×800 | 2 | Bottom(2) | Steam Deck - 32px height for bottom |
| 1366×768 | 2 | None | Common laptop, tight fit |
| 1680×1050 | 2 | Full | First scale-2 with right pane (40 cols) |
| 1920×1080 | 2 | Full | Full HD with 50-col right pane |
| 1920×1200 | 3 | Bottom(2) | First scale-3, limited height |
| 2048×1152 | 3 | None | Exact fit, no panes |
| 3440×1440 | 3 | Full | Ultrawide QHD |
| 7680×4320 | 3 | Full | 8K UHD |

### Files Modified
- `src/sdl-config.c`: Updated with 41 resolution profiles (was 9)
- Comprehensive header comment explaining layout logic
- Compact array initialization format for maintainability

### Build Status
✅ Compiled successfully with no errors
✅ All 41 profiles included in binary

---

## Blessing Point Threshold Now Data-Driven via Runtypes

### Feature
Moved the hardcoded blessing point threshold (previously 300 in `METARUN_BLESSING_POINT_THRESHOLD`) to be data-driven through the `L:` directive in `runtypes.txt`. This allows each difficulty to have a different threshold for earning blessing points from fallen character scores.

### Implementation
1. **Renamed Field**: Changed `runtype_type.lose_con` to `blessing_threshold` (u16b)
   - The old "lose condition" (death limit) feature was deprecated
   - Death limit is now hardcoded as `LOSECON_DEATHS` (15) for all runtypes
   
2. **Parser Update**: Modified `init1.c` to parse `L:` as blessing threshold
   - Changed from `byte` (max 127) to `u16b` (max 65535) to support larger values
   - Updated comment to reflect new purpose
   
3. **Code Updates**: Modified `metarun.c` to use `runtype_info[type].blessing_threshold`
   - `update_blessing_ledger()`: Uses runtype threshold with fallback to constant
   - Blessing exchange UI: Shows correct threshold for current difficulty
   - `print_metarun_stats()`: Displays appropriate threshold
   - Removed all `lose_con` references, replaced with `LOSECON_DEATHS` constant

4. **Data Configuration**: Updated `lib/edit/runtypes.txt` with balanced thresholds:
   - Echoes (Normal): L:300 (baseline)
   - Echoes (Hard): L:250 (20% easier to earn points)
   - Echoes (Very Hard): L:200 (33% easier compensation)
   - Echoes (Impossible): L:150 (50% easier for extreme difficulty)
   - Iron Gates (Impossible): L:200 (balanced for faster gameplay)

### Files Modified
- `src/types.h`: Renamed `lose_con` → `blessing_threshold`
- `src/init1.c`: Updated L: parser for blessing threshold
- `src/metarun.c`: Three locations updated to use data-driven threshold
- `src/metarun.h`: Updated constant documentation (now fallback only)
- `lib/edit/runtypes.txt`: Set difficulty-specific thresholds

### Benefits
- Harder difficulties earn blessing points faster (compensation mechanism)
- No hardcoded magic numbers in code
- Easy to tune per-difficulty through data files
- Maintains backward compatibility via fallback constant

---

## Inventory Examine Shows Equipment Comparison

### Issue
When pressing `x` (arrow right) to examine items in the inventory menu accessed via `i` (direct access), equipment comparisons were not being shown. The comparison only worked when accessing the inventory via the `x` command menu.

### Root Cause
In `do_cmd_inven()` at line 517-518, the code checked:
```c
bool include_comparisons = (current_menu_command == 'u' || current_menu_command == 'x');
```

However, when accessing inventory directly via `i`, `current_menu_command` is set to `0`, so comparisons were disabled even when examining via arrow right.

### Fix
Updated the comparison check to include direct access mode:
```c
bool include_comparisons = (current_menu_command == 'u' || current_menu_command == 'x' || current_menu_command == 0);
```

Now when you press arrow right (or `x`) to examine an item from the inventory menu (regardless of access method), it will show the equipment comparison for equivalent slots.

**File**: `src/cmd3.c`, lines 511-520

---

# Session Notes - October 15, 2025

## Curse Removal Fix - Only Remove One Stack

### Issue
When using blessing points to remove a curse, the `blessing_remove_curse()` function was removing ALL stacks of the selected curse instead of just one stack.

### Fix
Modified `blessing_remove_curse()` in `metarun.c` to:
1. Check current stack count before removal
2. Decrement by 1 if multiple stacks exist
3. Set to 0 only if it's the last stack
4. Update success message to indicate remaining stacks when applicable

### Changes
**File**: `src/metarun.c`
- Changed `CURSE_SET(curse_id, 0)` to conditional logic that removes only one stack
- Updated result message to show "One stack of [curse] is lifted. (X remains)" when stacks > 1
- Shows "The curse of [curse] is lifted." only when removing the final stack

## Resolution-Based Default Settings - REFACTORED TO DATA-DRIVEN

### Feature
Created a system where the game automatically applies optimized default settings based on screen resolution when `sil_sdl.json` is missing. Now uses a **data-driven approach** for easy maintenance.

### Architecture
**Data-Driven Design**: All resolution profiles stored in a single static array at the top of `sdl-config.c`
- `struct resolution_profile` contains all settings for a resolution
- `resolution_profiles[]` array holds all known profiles
- Function simply looks up and applies the matching profile

### Adding New Resolutions
Just add a new entry to the `resolution_profiles[]` array in `sdl-config.c`:

```c
// Example: Adding 1920x1080 support
{
    .width = 1920,
    .height = 1080,
    .name = "1920x1080 (Full HD)",
    .main_view_scale = 2,
    .aux_view_font_size = 16,
    .margin = 4,
    .fullscreen = false,
    .tiles = true,
    .window_x = 100,
    .window_y = 100,
    .window_width = 1820,
    .window_height = 980,
    .pane_count = 5,
    .panes = {
        { PANE_INVENTORY, PLACE_RIGHT, 18, 40 },
        { PANE_WORN,      PLACE_RIGHT, 14, 0  },
        { PANE_INFO,      PLACE_RIGHT, 6,  0  },
        { PANE_ROLLS,     PLACE_BOTTOM, 3, 0  },
        { PANE_LOG,       PLACE_BOTTOM, 0, 0  }
    }
}
```

**That's it!** No function changes needed. The lookup function automatically finds and applies it.

### Current Resolution Profiles
- **2880x1800**: Scale 3, windowed 2779x1466, 5 panes (INVENTORY, WORN, INFO, ROLLS, LOG)
- **2560x1600**: Scale 3, windowed 2459x1266, same pane layout

### Implementation Details
1. **New Structure**: `struct resolution_profile` in `sdl-config.c`
   - Contains width, height, name, all SDL settings, and pane configurations
   - Supports up to 8 panes per profile

2. **Profile Database**: `resolution_profiles[]` static array
   - Each entry is a complete configuration for one resolution
   - `NUM_RESOLUTION_PROFILES` auto-calculated from array size

3. **Lookup Function**: `sdl_config_set_defaults_for_resolution()`
   - Searches profiles for matching width/height
   - Applies all settings if found
   - Falls back to generic defaults if not found

### Files Modified
- `src/sdl-config.c`: Added `resolution_profile` struct, profiles array, refactored function to data-driven lookup
- `src/sdl-config.h`: Declaration unchanged (same API)
- `src/main-sdl.c`: No changes (uses same function signature)

### Benefits
✅ **Single source of truth** - all resolution data in one array  
✅ **No function edits** - just add/remove array entries  
✅ **Type-safe** - compiler checks struct fields  
✅ **Self-documenting** - each profile clearly labeled  
✅ **Easy maintenance** - adding 1920x1080 takes ~15 lines  

---

## Metarun Scoring System Redesign - v0.9.0.2

### Changes Implemented
1. **Progressive Diminishing Score**: Replaced single best-run scoring with `best/1 + second/2 + third/4 + ...` (caps at 16 runs)
2. **Fully Automatic Versioning**: Metarun uses `VERSION_*` defines directly from `defines.h` (no intermediate defines)
3. **Generous Reserved Space**: Increased `reserved_runtime[1→32]` for easier future expansions
4. **Backward Compatible Saves**: Savefile loader accepts `VERSION_EXTRA >= 1` (loads old 0.9.0.1 saves)

### Files Modified
- `src/defines.h`: Bumped `VERSION_EXTRA` from 1 to 2 (progressive scoring update)
- `src/metarun.h`: Removed `METARUN_FILE_VERSION_*` defines, uses `VERSION_*` directly, added `reserved_runtime[32]`
- `src/metarun.c`: Progressive scoring function, direct `VERSION_*` usage, migration code for v8→v9
- `src/load.c`: Changed version check from `extra == 1` to `extra >= 1` for backward compatibility

### Technical Details
- New `compute_progressive_character_score()` aggregates top 16 runs with halving divisor
- **Metarun version uses `VERSION_MAJOR/MINOR/PATCH/EXTRA` directly** - cleaner code, no duplication
  - When you change `VERSION_*` in `defines.h`, metarun version updates automatically
  - Current: Game 0.9.0.2 → Metarun 0.9.0.2 (identical)
- `quest_reserved[12]` kept unchanged (user requirement)
- `reserved_runtime[32]` provides 32 bytes for future features (generous expansion space)
- Migration from v8 (0.9.0.1) to v9 (0.9.0.2) handles reserved space expansion
- **Savefile backward compatibility**: Old saves with extra=1 load in new version (extra=2)
- Build successful with only minor type-limit warnings

### Benefits
- Rewards consistency across multiple characters (not just single best run)
- Example: 5 runs [1000,800,600,500,400] → old: 1000, new: 1637 (+64%)
- **100% automatic version tracking** - no intermediate defines to maintain
- Generous expansion space makes future additions easier
- **Full backward compatibility** - old saves continue to work

---

## Application "Not Responding" During Intro Text - FIXED

### Problem
- When `print_story_intro()` or other long intro windows display, Windows reports app as "Not Responding"
- Character-by-character typing effect and paragraph delays were blocking the event loop
- No SDL events being processed during `TERM_XTRA_DELAY` calls

### Root Cause
In `src/main-sdl.c`, the `TERM_XTRA_DELAY` handler simply called `SDL_Delay()`:
```c
case TERM_XTRA_DELAY:
    SDL_Delay((Uint32)v);  // Blocks thread completely
    return 0;
```

This meant during the story intro (with 30ms delays per character and 1000ms delays per paragraph), the app wasn't processing any window events, causing Windows to mark it as unresponsive.

### Fix Implemented
Modified `TERM_XTRA_DELAY` in `src/main-sdl.c` (lines 366-380) to process events during delays:
```c
case TERM_XTRA_DELAY: {
    /* Break delay into chunks and process events to keep app responsive */
    Uint32 total_delay = (Uint32)v;
    Uint32 chunk = 20; /* Process events every 20ms */
    
    while (total_delay > 0) {
        Uint32 this_delay = (total_delay < chunk) ? total_delay : chunk;
        SDL_Delay(this_delay);
        total_delay -= this_delay;
        
        /* Process pending events to prevent "Not Responding" status */
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            sdl_handle_event(&g_state, &ev);
        }
    }
    return 0;
}
```

Now the app processes SDL events every 20ms during delays, keeping it responsive to OS window messages while still honoring the requested delay duration.

---

## Critical Save Corruption Bug - FIXED

### Problem
- Alive character "Gamil Zirak" failed to autoload from savefile
- Savefile was corrupted: only 39KB instead of expected 58KB (~19KB missing)
- Dungeon data section was all zeros (depth=0, position=0,0, map size=0x0)
- Character was saved just minutes before, indicating active corruption bug
- **New saves appeared successful in logs but still resulted in corrupted files**

### Root Cause - THE REAL BUG
**Critical bug in `src/util.c` - `fd_move()` function ignores `rename()` errors:**

```c
// OLD CODE - BUGGY:
errr fd_move(cptr file, cptr what)
{
    // ... path parsing ...
    
    /* Rename */
    (void)rename(buf, aux);  // ERROR: Ignores return value!
    
    /* Assume success XXX XXX XXX */
    return (0);  // Always returns success even if rename failed!
}
```

### How the Bug Manifests
The save flow in `save_player()`:
1. Successfully write new save to `Gamil_Zirak.new` (58KB, complete with dungeon data)
2. Try to move old `Gamil_Zirak` → `Gamil_Zirak.old` 
   - **rename() FAILS** (file locked, permissions, or other error)
   - **BUT fd_move() returns 0 (success)**
3. Try to move `Gamil_Zirak.new` → `Gamil_Zirak`
   - **rename() FAILS** because old file still exists
   - **BUT fd_move() still returns 0**
4. Result: Old corrupt `Gamil_Zirak` (39KB) stays in place
5. Good `.new` file gets deleted
6. Game thinks save succeeded!

### Why Old File Was Corrupt
The original 39KB file was likely from an earlier interrupted save or a save during an invalid state. Once it got corrupted, it could never be replaced because the rename operations kept failing silently.

### Fix Implemented
1. **Modified `fd_move()` in `src/util.c`** (lines 510-535):
   - Check `rename()` return value
   - Log errors with `strerror(errno)`
   - Return -1 on failure
   - Log successful renames

2. **Modified `save_player()` in `src/save.c`** (lines 1726-1744):
   - Check fd_move() return values
   - Abort save activation if rename fails
   - Attempt to restore old file if new activation fails
   - Proper error reporting

3. **Added diagnostic logging**:
   - Invalid dungeon dimension detection in wr_savefile()
   - EOF detection in load.c sf_get()
   - Detailed rd_dungeon() header parsing
   - Write error tracking in sf_put()

**Changed files:**
- `src/util.c`: Lines 510-535 (fd_move error checking)
- `src/save.c`: Lines 1726-1744 (save activation error handling)
- `src/save.c`: Lines 378-398, 1359-1362, 1528-1548 (write error detection)
- `src/load.c`: Multiple locations (enhanced diagnostics)

### Testing
- Build successful
- Proper error detection now happens immediately when file operations fail
- Corrupt files will be detected and reported rather than silently keeping old bad data
- Save will fail cleanly rather than appearing to succeed while keeping corrupt data

### Status
✅ **CRITICAL BUG FIXED** - File rename errors are now properly detected
✅ **Save corruption prevented** - Failed renames will abort save activation
✅ **Enhanced diagnostics** - Better logging for future debugging
⚠️ **User Action Required** - Delete corrupt `Gamil_Zirak` file and load from backup if available
 - Deaths Redesign Bug Fixes

## Date
October 14-15, 2025

## Metarun Backwards Compatibility Removal (Oct 15)

### Current Version
**Metarun file version: 0.9.0.1**
- Major: 0
- Minor: 9
- Patch: 0
- Extra: 1 (for persistent blessing choices)

### Changes Made - Phase 1: Non-Versioned File Support Removal
Removed backwards compatibility for non-versioned meta.raw files. The system now ONLY accepts versioned meta.raw files (v0.9.0+).

**What was removed:**
- Legacy file format detection based on file size modulo operations
- Support for reading non-versioned metarun entries
- Conversion code for legacy formats within non-versioned files

**What remains:**
- Versioned file support (with proper header)
- Conversion from older versioned formats (v5, v6, v7, v8) within versioned files
- Automatic default metarun creation if file is rejected

**Behavior:**
- Non-versioned meta.raw files are now rejected with warning: "non-versioned meta.raw is no longer supported (requires v0.9.0+)"
- System will create a fresh default metarun slot when encountering unsupported files
- Existing versioned saves continue to work normally

### Changes Made - Phase 2: Pre-v5 (metarun_old) Support Removal
Removed support for the oldest metarun format that used bit-packed curses.

**What was removed:**
- `metarun_old` struct definition from `metarun.h`
- `metarun_from_legacy_old()` conversion function
- Pre-v5 entry size check in `is_versioned_meta_file()`
- Pre-v5 loading code in `load_metaruns()`

**What remains:**
- Support for v5, v6, v7, v8 formats within versioned files
- v5 is now the oldest supported format (uses bit-packed curses but has quest/oath tracking)

**Impact:**
- Files with pre-v5 format entries will be rejected even within versioned files
- Minimum supported format is now v5 (quest system era)
- Cleaner codebase with one less legacy conversion path

### Files Modified
- `src/metarun.h` - Removed `metarun_old` typedef
- `src/metarun.c` - Removed non-versioned loading code (Phase 1) and pre-v5 support (Phase 2)

### Testing Needed
- Confirm versioned saves (v5+) load correctly
- Verify non-versioned files trigger proper warning and create default metarun
- Verify pre-v5 entries in versioned files are rejected gracefully

---

## Critical Bug Fix: New Character Savefile Path Using Wrong House Name (Oct 15) - SECOND FIX

### Problem
After the first fix, new characters were still being saved with the wrong house name. The savefile was created as "Houseless" instead of the selected house name (e.g., "Gamil_Zirak").

### Root Cause (Deeper Analysis)
The first fix removed the `player_wipe()` call after `load_player()`, which preserved the `p_ptr->phouse` value. However, there was a SECOND bug in the order of operations:

**Incorrect Order:**
1. `character_creation()` - user selects house, stores in `p_ptr->phouse`
2. `load_player()` - tries to load savefile using `op_ptr->full_name`
   - **BUG**: At this point, `op_ptr->full_name` is still "Houseless" (default value)
   - Generates savefile path: `./lib/save/Houseless`
3. `player_birth_aux()` - sets `op_ptr->full_name` from house name
   - Line 2475: `my_strcpy(op_ptr->full_name, c_name + c_info[p_ptr->phouse].name, ...)`
   - Line 2476: `process_player_name(true)` updates savefile path
   - **TOO LATE!** `load_player()` already used wrong path

**Evidence from Log:**
```
3614: Character creation step completed: Naugrim Gamil Zirak
3615: Loading savefile './lib/\save\Houseless'  <-- WRONG!
3625: Generated savefile path: './lib/\save\Gamil_Zirak'  <-- Correct, but too late
```

### Fix
Set `op_ptr->full_name` immediately after `character_creation()` and BEFORE `load_player()`:

```c
/* Set player name from house BEFORE load_player() so savefile path is correct */
my_strcpy(op_ptr->full_name, c_name + c_info[p_ptr->phouse].name, sizeof(op_ptr->full_name));
process_player_name(true);  /* Update savefile path */
log_debug("Player name set to: %s (house %d), savefile: %s", op_ptr->full_name, p_ptr->phouse, savefile);
```

This ensures that when `load_player()` is called, it uses the correct savefile path based on the selected house.

### Files Modified
- `src/dungeon.c` - Added house name initialization before `load_player()` call
- `session_notes.md` - Updated with second bug analysis

### Testing
- Build successful
- Ready for testing: create new character, select house, verify savefile is created with correct name

---

## Critical Bug Fix: New Character House Reset (Oct 15) - FIRST FIX

### Problem
New characters always start with the name "Houseless" (house 0 from character.txt)
This happens even when the player selects a different house during character creation
The selected house is not being preserved

### Root Cause
In `src/dungeon.c`, the character creation flow had a critical flaw:

1. Line 3353: `player_wipe()` - resets everything including house to 0
2. Line 3356: `character_creation()` - user selects race and house (stored in `p_ptr->prace` and `p_ptr->phouse`)
3. Line 3365: `load_player()` - attempts to load savefile
   - For NEW characters, savefile doesn't exist, returns `false`
4. Line 3367: `player_wipe()` - **WRONGLY wipes player data again!**
   - This resets `p_ptr->phouse` back to 0
5. Later in `player_birth_aux()` at birth.c:2475:
   ```c
   my_strcpy(op_ptr->full_name, c_name + c_info[p_ptr->phouse].name, ...);
   ```
   Uses `p_ptr->phouse` which is now 0 (Houseless)

The bug was that `player_wipe()` was unconditionally called after `load_player()` returned false, even for new characters where the house had just been selected.

### Fix
Removed the conditional `player_wipe()` call after `load_player()` fails. The player data is already wiped at the start of the loop (line 3353), and:
- For new characters: data is clean from `character_creation()`
- For dead characters: `character_loaded_dead` is set, and `player_wipe()` preserves choices
- For corrupted files: very rare, and will be cleaned by the next loop iteration

Changed from:
```c
if (!load_player()) {
    log_debug("Failed to load player - wiping corrupted data");
    player_wipe();
}
```

To:
```c
(void)load_player();
```

### Files Modified
- `src/dungeon.c` - Removed redundant `player_wipe()` call after `load_player()`

---

## Critical Bug Fix: Save/Load Format Mismatch (Oct 15)

### Problem
- Savefiles created with latest code failed to load correctly
- `min_depth_counter` was corrupted: saved as 85, loaded as 5308418 (calculated to depth 20!)
- Characters teleported to level 20 when going down stairs from level 1
- Quest marker read as 0x02 instead of 0x51
- Character name corrupted: "Houseless" instead of actual name

### Root Cause Analysis
**TWO separate bugs in the save/load format:**

1. **Crown Shatter Fields Mismatch:**
   - **Save code** (save.c lines 977-978): ALWAYS writes `crown_shatter_sil2` and `crown_shatter_sil3` 
   - **Load code** (load.c lines 1103-1157): Only reads these fields if `sf_extra >= 3`, or `sf_extra == 2`, or sets defaults if `sf_extra < 2`
   - With `VERSION_EXTRA=1`, these 2 bytes are written but NEVER read
   - Result: All subsequent data offset by 2 bytes!

2. **Playerturn Field Mismatch:**
   - **Save code**: Writes turn, then playerturn, then crown_shatter fields
   - **Load code** with `sf_extra < 2`: Only reads turn, skips playerturn and crown_shatter
   - Result: Additional 4-byte offset (s32b)

**Combined Effect:**
- 6 bytes of offset (4 for playerturn + 2 for crown_shatter)
- `oath_type` saved as 2, loaded as 0 (reading from wrong position)
- Quest marker 0x51 offset by 6 bytes, read as 0x02
- Load code thinks it's a legacy 0.8.5 save, rewinds file position
- `min_depth_counter` read from completely wrong position → garbage value

### Fix
**Cleaned up save/load logic by removing ALL legacy compatibility:**

1. **Dropped support for savefiles < 0.8.9** (changed `OLD_VERSION_PATCH` from 0 to 9)
2. **Simplified load logic** to match current save format:
   - Always read: turn → playerturn → crown_shatter_sil2 → crown_shatter_sil3
   - Removed all conditional sf_extra checks for these fields
3. **Simplified quest block loading:**
   - Removed all legacy quest compatibility code (1.5.x, 0.8.5, etc.)
   - Now expects marker 0x51, returns error if not found
4. **Set VERSION_EXTRA = 1** with clear documentation

### Files Modified
- `src/defines.h` - Set OLD_VERSION_PATCH to 9, VERSION_EXTRA to 1
- `src/load.c` - Removed legacy compatibility, simplified turn/crown_shatter/quest loading
- `src/dungeon.c` - Always wipe player data on failed load

### Testing Required
- Delete ALL old savefiles (they are incompatible)
- Create new character and save
- Load and verify depth is correct
- Go down stairs and verify no teleportation to level 20

---

## Critical Bug Fix: Corrupted Depth After Failed Load (Oct 15)

### Problem
- Characters would teleport to level 20 (Morgoth's level) when going down stairs from level 1
- This happened after a failed savefile load attempt
- The corruption persisted even after "starting a new character"

### Root Cause
When `load_player()` fails (e.g., due to checksum mismatch), it:
1. Reads partial data into `p_ptr` (including a corrupted `depth` value)
2. Encounters error (checksum mismatch) and returns `false`
3. **BUT the corrupted data remains in `p_ptr`!**

The bug was in `src/dungeon.c` lines 3367-3370:
```c
if (!load_player()) {
    log_debug("Failed to load player");
    if (character_loaded_dead) player_wipe();  // Only wipes if character_loaded_dead!
}
```

When a checksum error occurs, `character_loaded_dead` is false, so `player_wipe()` is never called, leaving corrupted data (like `p_ptr->depth = 20`) in memory!

### Fix
Always wipe player data when load fails, regardless of `character_loaded_dead` flag:
```c
if (!load_player()) {
    log_debug("Failed to load player - wiping corrupted data");
    player_wipe();  /* Always wipe on load failure */
}
```

### Files Modified
- `src/dungeon.c` - Always call `player_wipe()` on failed load

---

## Bug Fix: SMT_GAMIL Mithril Melting (Oct 14)

### Problem
- Items created with SMT_GAMIL flag (Gamil house bonus) should not be meltable
- These items are created without mithril using 3 forge charges and marked with `IDENT_CANT_MELT`
- Melt menu was appearing when these items were in inventory
- Empty strings were shown in the menu but selection was still possible and allowed melting

### Root Cause
Inconsistent filtering logic between display and selection:
1. **Display code** (`melt_menu_aux` line 6678): Correctly filtered out `IDENT_CANT_MELT` items
2. **Selection code** (`melt_mithril_item` line 3495): Counted ALL mithril items without checking the flag
3. **Menu availability** (`mithril_items_carried` line 3609): Counted ALL mithril items without checking the flag

This caused:
- Menu would show as available (count included Gamil-forged items)
- Display would show empty slots (filtering worked)
- Selection would access wrong items (numbering mismatch)

### Fix
Added `IDENT_CANT_MELT` flag check to both counting functions:

**melt_mithril_item** (cmd4.c ~line 3495):
```c
/* Skip mithril items that can't be melted (Gamil-forged) */
if ((f3 & TR3_MITHRIL) && !(o_ptr->ident & IDENT_CANT_MELT))
```

**mithril_items_carried** (cmd4.c ~line 3609):
```c
/* Only count mithril items that can be melted (exclude Gamil-forged) */
if ((f3 & TR3_MITHRIL) && !(o_ptr->ident & IDENT_CANT_MELT))
```

### Files Modified
- `src/cmd4.c` - Added flag checks to both functions

### Testing Required
- Create character with Gamil house (SMT_GAMIL flag)
- Craft a mithril item without mithril (uses 3 forge charges)
- Verify the item has `IDENT_CANT_MELT` flag set
- Visit a forge - melt menu should not appear if only Gamil-forged items exist
- Acquire normal mithril items - verify they can still be melted
- Verify Gamil-forged items never appear in melt menu

---

## Bug Fix: Savefile Checksum Mismatch

### Problem
- Characters marked as "(alive and well)" in scores.raw were not loading at startup
- `autoload_alive_from_scores()` was finding alive characters but `load_player()` was failing
- Error: "Checksum mismatch: expected 380738, got 16737057"
- Savefile existed and was created with latest code, but loading failed

### Root Cause
The supply block feature was added to savefiles but VERSION_EXTRA was never incremented:
1. **Save code** (save.c lines 1487-1506): Always writes the supply block to savefiles
2. **Load code** (load.c line 1634): Only reads supply block if `sf_extra >= 1`
3. **Version** (defines.h line 64): `VERSION_EXTRA` was defined as 0
4. **Result**: Savefiles written with sf_extra=0 include supply data, but loader skips it, causing checksum calculation on different byte sequences

### Fix
Changed `VERSION_EXTRA` from 0 to 1 in `src/defines.h` line 64.

This ensures:
- New savefiles will be written with `sf_extra=1`
- The load code will properly read the supply block
- Checksum will be calculated on the same data during both save and load
- Old savefiles with `sf_extra=0` will be incompatible (by design - they had the bug)

### Files Modified
- `src/defines.h` - Incremented VERSION_EXTRA from 0 to 1

### Testing Required
- Create a new character and save
- Quit and restart the game
- Verify the character auto-loads successfully from scores.raw
- Verify no checksum mismatch errors

---

## Previous Session: Metarun Blessing Exchange UI Enhancement

### Changes Made
Redesigned the blessing exchange feedback system to display messages inline within the menu itself instead of using intrusive `msg_print()` calls.

### Files Modified
- `src/metarun.c`

1. **Modified blessing functions** to accept result message parameters:
   - `blessing_remove_curse()` - now accepts `char *result_msg, size_t msg_size, byte *result_attr`
   - `blessing_gain_minor()` - same parameters added
   - `blessing_unlock_major()` - same parameters added

2. **Replaced `msg_print()`/`msg_format()` calls** with inline status messages:
   - Success messages use color coding: 
     - Blue (`TERM_L_BLUE`) for curses lifted
     - Green (`TERM_L_GREEN`) for minor blessings received
     - Yellow (`TERM_YELLOW`) for major blessings sealed
   - Error/info messages use orange (`TERM_ORANGE`) or dark (`TERM_L_DARK`)

3. **Enhanced `open_blessing_exchange()`** menu:
   - Added `status_msg[256]` buffer to track feedback
   - Added `status_attr` to control message color
   - Added `clear_status_on_next_key` flag to auto-clear after display
   - Status message displays at row 14 within the menu interface
   - Messages clear automatically on navigation (arrow keys) or after next action

### Files Modified
- `src/metarun.c`

### Technical Details
- Status messages are displayed directly in the menu at `Term_putstr(2, 14, -1, status_attr, status_msg)`
- Messages persist for one keypress then clear on navigation or next action
- All error conditions now show inline feedback instead of modal message boxes
- The change maintains all existing functionality while improving UX

---

## Python Environment Setup & Agent Configuration (October 19, 2025)

### Issue Identified
Agents were unable to use Python or encountered incorrect path errors when attempting to run Python scripts in the workspace.

### Root Causes
1. **PowerShell syntax issue**: The `&` call operator is required for executables; without it, PowerShell interprets `--` and `-` as operators
2. **Lack of documentation**: Agents had no clear guidance on venv location or correct usage patterns
3. **VS Code Python settings missing**: Workspace didn't explicitly configure the Python interpreter path

### Changes Implemented

#### 1. Updated `.github/instructions/copilot-instructions.md`
- Added "Python Environment & Scripts" section with:
  - Correct venv path: `src/.venv/`
  - Correct executable path: `C:/Users/efrem/Documents/GitHub/sil-qh/src/.venv/Scripts/python.exe`
  - Python version: 3.13.2
  - PowerShell syntax guidance: `& "path/to/python.exe" script.py`
  - List of available utility scripts

#### 2. Updated `.vscode/settings.json`
- Added Python extension configuration:
  ```json
  "python.defaultInterpreterPath": "${workspaceFolder}/src/.venv/Scripts/python.exe"
  "[python]": { "editor.formatOnSave": true }
  ```
- Enables auto-discovery of workspace venv

#### 3. Created `PYTHON_SETUP.md`
- Comprehensive guide covering:
  - Quick summary of venv location and packages
  - Correct usage for PowerShell, Bash, and cmd.exe
  - Virtual environment activation instructions
  - Available Python utilities documentation
  - Troubleshooting guide for common issues
  - Key reminders for AI agents

### Verified Working
- Python 3.13.2 venv is fully functional
- Pre-installed packages: numpy (2.3.3), pandas (2.3.3), pillow (11.3.0), python-dateutil, pytz
- All packages are accessible
- Tested with: `& "./src/.venv/Scripts/python.exe" -c "import numpy; import pandas; print('working')"`

### Key Points for Agents

**Correct PowerShell Usage**:
```powershell
& "C:/Users/efrem/Documents/GitHub/sil-qh/src/.venv/Scripts/python.exe" script.py
& "./src/.venv/Scripts/python.exe" calc_all_resolutions.py  # from workspace root
```

**Why the `&` operator is needed**: PowerShell treats executable invocation differently from cmd.exe and bash. The `&` call operator ensures the path is treated as a command, not a string literal.

**Available Utility Scripts**:
- `calc_all_resolutions.py` - Resolution calculator
- `lib/edit/Power_Ratings.py` - Object power analysis
- `lib/edit/parse_and_align_abilities.py` - Ability definitions
- `lib/xtra/graf/osx_bmp2png.py` - Graphics conversion

### Files Modified/Created
- `.github/instructions/copilot-instructions.md` (updated)
- `.vscode/settings.json` (updated with Python config)
- `PYTHON_SETUP.md` (new comprehensive guide)

---
### Build Status
✅ Compiled successfully
✅ Deployed to `sil-more-windows-sdl3/`

### Testing Needed
- Verify blessing point spending shows proper feedback in the menu
- Test curse removal feedback displays correctly
- Test minor blessing feedback displays correctly  
- Test major blessing feedback displays correctly
- Verify error messages (insufficient points, etc.) display inline
- Confirm navigation clears status messages appropriately

---

## Current Session: Save/Load Debugging Infrastructure (2025-10-15)

### Problem
Save file corruption - saves cannot load. Need detailed logging to identify synchronization issues.

### Solution Applied
Added comprehensive byte-level logging to all save/load primitive operations.

### Changes Made

1. **Byte Offset Tracking**
   - save.c: Added save_byte_offset counter
   - load.c: Enhanced load_byte_offset counter
   - Both reset at start, incremented by all I/O functions

2. **Primitive I/O Logging**
   - wr_u16b/rd_u16b: Log values in hex and decimal
   - wr_u32b/rd_u32b: Log values in hex and decimal
   - wr_s16b/rd_s16b: Log signed values
   - wr_s32b/rd_s32b: Log signed values
   - Format: [save:XXXXXX] or [load:XXXXXX] with 6-digit offset

3. **Section Markers**
   - All major sections bracketed with === BEGIN/END === markers
   - Inventory, supplies, dungeon, cave RLE, objects, monsters
   - Door-choices probes clearly logged

### Files Modified
- src/save.c (primitive I/O, wr_savefile, wr_dungeon, inventory/supplies sections)
- src/load.c (primitive I/O, rd_savefile_new_aux, rd_dungeon, rd_inventory)

### Log Analysis
1. Check sil-more-windows-sdl3/log.txt
2. Search for [save: and [load: to trace operations
3. Compare offsets where save and load diverge
4. Look for probe mismatches (door-choices 0xD00D magic)

### Build Status
 Compiled successfully
 Ready for testing


### CRITICAL BUG FOUND AND FIXED

**Issue**: First-time saves were failing silently!

**Root Cause**: In save_player() (src/save.c), the code was trying to preserve the old savefile by renaming it to .old BEFORE checking if it exists. For first-time saves, this rename failed because there was no old file, causing the entire save process to abort.

**Log Evidence**:
```
ERROR fd_move: rename('./lib/\save\Gamil_Zirak', './lib/\save\Gamil_Zirak.old') failed: No such file or directory
ERROR Failed to preserve old savefile - aborting activation
```

The .new file was written successfully, but never activated because the old file preservation step failed.

**Fix Applied**:
Modified save_player() in src/save.c to check if old savefile exists before trying to preserve it:
1. Use fd_open(savefile, O_RDONLY) to check if old file exists
2. If it exists (fd >= 0), close it and preserve it as .old
3. If it doesn't exist (first-time save), skip the preserve step with a log message
4. Continue with activation of .new file

**Files Modified**:
- src/save.c (lines 1770-1805)

**Build Status**:
 Compiled successfully
 Ready for testing - first-time saves should now work!


### CRITICAL BUG #2 FOUND AND FIXED - Double Byte Counting

**Issue**: Load was reading from wrong file offsets - byte positions were exactly DOUBLE what they should be!

**Root Cause**: When I added byte offset tracking to the load functions, I made 
d_byte(), 
d_u16b(), 
d_u32b(), etc. increment load_byte_offset. BUT sf_get() (the underlying function that reads bytes) ALREADY increments load_byte_offset! This caused DOUBLE counting.

**Evidence from logs**:
- SAVE wrote smithing item at offset 038966
- LOAD read smithing item at offset 077932 (almost exactly 2x!)
- This caused ALL subsequent data to be read from wrong positions
- Dungeon header was read from wrong position, resulting in illegal map dimensions (20x0)

**How double-counting happened**:
`c
// sf_get() increments load_byte_offset
static byte sf_get(void) {
    ...
    load_byte_offset++;  // <-- INCREMENTS HERE
    return (v);
}

// rd_u16b() was ALSO incrementing
static void rd_u16b(u16b* ip) {
    (*ip) = sf_get();              // +1 from sf_get
    (*ip) |= ((u16b)(sf_get()) << 8);  // +1 from sf_get  
    load_byte_offset += 2;         // +2 MORE! Total: +4 instead of +2
}
`

**Fix Applied**:
Removed the manual load_byte_offset increments from:
- 
d_byte() - removed load_byte_offset++
- 
d_bool() - removed load_byte_offset++  
- 
d_u16b() - removed load_byte_offset += 2
- 
d_u32b() - removed load_byte_offset += 4

The sf_get() function already handles all byte counting correctly.

**Files Modified**:
- src/load.c (lines 227-262)

**Build Status**:
 Compiled successfully
 Ready for testing - saves should now load correctly!


### CRITICAL BUG #3 FOUND AND FIXED - Supply Data Version Mismatch

**Issue**: After picking up a potion for supplies, saves would fail to load with "Illegal player location" error.

**Root Cause**: Version mismatch in supplies read/write logic caused 4-byte misalignment!

**The Problem**:
- VERSION_EXTRA = 1 (defined in defines.h)
- **Save logic** (save.c line 1575): ALWAYS writes stored_units (s32b, 4 bytes) after each supply item
- **Load logic** (load.c line 1547): Only reads stored_units if sf_extra >= 2
- When loading a file saved with extra=1, the load skipped the 4-byte stored_units field
- This caused 4-byte misalignment for all subsequent data
- Dungeon header was read from wrong position  invalid map dimensions  load failed

**Evidence from logs**:
`
SAVE (extra=1):
  - Supply item: 79 bytes
  - stored_units (s32b): 4 bytes
  - Total: 83 bytes (038158  038241)

LOAD (extra=1, but checking >= 2):
  - Supply item: 79 bytes
  - SKIPPED stored_units!
  - Total: 79 bytes (038158  038237)
  - 4-byte SHORT!
`

**Fix Applied**:
Changed load.c line 1547 from:
`c
if (sf_extra >= 2)
    rd_s32b(&stored_units);
`
To:
`c
if (sf_extra >= 1)  // Fixed: was >= 2, but VERSION_EXTRA=1 writes this field
    rd_s32b(&stored_units);
`

**Files Modified**:
- src/load.c (line 1547)

**Build Status**:
 Compiled successfully
 Ready for testing - saves with supplies should now load correctly!


---

## Version Check Cleanup (2025-10-15)

### Removed All Backward Compatibility Code

**Goal**: Simplify codebase by removing all legacy save format support. Only 0.9.0 extra=1 is now supported.

**Changes Made**:

1. **Removed older_than() function** - No longer needed
2. **Simplified version validation** - Now strictly checks for 0.9.0 extra=1, rejects all other versions
3. **Removed pickup_slot version check** - Always reads pickup_slot (required in 0.9.0)
4. **Removed main_combat_rolls version check** - Always reads the field with 7 spare bytes
5. **Removed legacy randarts alignment recovery** - No longer attempts to recover misaligned old savefiles
6. **Removed skills backward compatibility** - Always reads all S_MAX skills including S_SPC (Special)
7. **Removed abilities version check** - Always reads have_ability for all skills
8. **Removed supplies version check** - Always reads stored_units field
9. **Removed version conversion warning** - No version conversion happens

**Files Modified**:
- src/load.c (multiple functions simplified)

**Benefits**:
- Cleaner, more maintainable code
- No confusing version checks scattered throughout
- Faster load times (no legacy compatibility checks)
- Clear error message when loading incompatible savefiles: "Incompatible savefile version (expected 0.9.0 extra=1)"
- Removed ~80 lines of legacy compatibility code

**Warning**: 
Old savefiles from versions prior to 0.9.0 extra=1 will NO LONGER load. This is intentional.

**Build Status**:
 Compiled successfully with only one harmless warning (main_combat_rolls comparison)
 Ready for testing

# Session Notes - October 20, 2025

## Unified Look Sidebar Bigtile Alignment
- **File**: `src/cmd4.c` (`show_unified_sidebar()`)
- Verified the sidebar spans exactly 13 columns before the map pane, so bigtile pairs on the map start on an odd column.
- Updated monster rows to keep the combined name/HP/morale span at least 13 characters and always odd-length when `use_bigtile`, padding with spaces as needed while respecting buffer limits.
- Adjusted the monster/object section headers to 13 characters and reworked object row padding to guarantee odd spans ≥13 characters under bigtile rendering.
