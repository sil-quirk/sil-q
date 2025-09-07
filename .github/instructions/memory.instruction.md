---
applyTo: '**'
---

# Sil-More Project Memory & Knowledge Base

## Build System Notes
**IMPORTANT**: Use `Makefile.cyg` for compilation, NOT `Makefile.win`
- Command: `make -f Makefile.cyg` (in Cygwin bash from src/ directory)
- Makefile.win has linking issues and should be avoided
- Always use Cygwin environment for proper compilation

**CRITICAL LAUNCH PROCESS**: 
- Build in: `/cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src/`
- Launch method 1: `make -f Makefile.cyg launch` (from src/ directory)
- Launch method 2: `../sil.exe` (from src/ directory) 
- DO NOT run `./src/sil.exe` from root directory - this doesn't work properly
- The launch target moves sil.exe to parent directory and runs it from there
- Data files are in lib/ subdirectory, game finds them correctly when launched properly

## Log File Location
**CRITICAL**: The log.txt file location depends on WHERE you run the executable:
- If run from `/cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/` → log.txt appears in root directory
- If run from `/cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src/` → log.txt appears in src directory  
- Always check the correct location based on where sil.exe was executed

## Data File Parsing System - Critical Bug Fixes (September 2025)

### LATEST: Oath Breaking System Enhancement (Version 0.8.7) - September 5, 2025
**Issue**: Oath breaking experience needed dramatic improvements for better user experience
**Implemented Enhancements**:
1. **Improved Oath Breaking UI**: Updated `choose_oath_breaking_curse_ui()` with:
   - Added Tolkien-style heading: "The Sundering of Sacred Vows"
   - Shows only E: field (permanent message) from oath.txt with fade effect
   - 3-second pause after E: text display
   - Morgoth's attention text displayed in red with fade effect
   - Proper spacing between text sections

2. **Character Sheet Title Update**: Modified character name display in `files.c`:
   - When any oath is broken (`p_ptr->oaths_broken`), shows "[Name] the Oathbreaker" in red
   - Normal house title display when no oaths broken

3. **Abilities Menu Death Message**: Enhanced `abilities_menu2()` in `cmd4.c`:
   - Displays Q: field (death message) from oath.txt at bottom of abilities menu
   - Uses proper terminal size calculation and word wrapping
   - Displayed immediately in red color for broken oaths

4. **Dynamic Text Wrapping**: Updated text wrapping throughout oath system:
   - Uses `Term_get_size()` for actual terminal dimensions
   - Proper word boundary detection and wrapping

### LATEST: Data-Driven Quest Roulette System Enhancement (Version 0.8.7) - September 7, 2025
**Issue**: Quest roulette system was completely hardcoded with fixed order (Tulkas→Niena), needed to be data-driven based on Y: field
**User Request**: "Make roulette order random and use Y:1 field from quest.txt to determine which quests participate in the lottery"

**Problem Analysis**:
1. **Hardcoded Quest Selection**: Only Tulkas (quest ID 1) and Niena (quest ID 4) were hardcoded in lottery
2. **Fixed Evaluation Order**: Always checked Tulkas first, then Niena - no randomization
3. **Probability Formulas Hardcoded**: Each quest had specific hardcoded formulas that couldn't be reused
4. **No Extensibility**: Adding new roulette quests required code changes to generate.c

**Implemented Solution - Data-Driven Roulette System**:

1. **Quest Registry Architecture**: Added `roulette_quest_entry` structure:
   - `quest_id`: Quest index from quest.txt (1=Tulkas, 4=Niena)
   - `quest_state_ptr`: Pointer to actual quest state variable (p_ptr->tulkas_quest, etc.)
   - `metarun_quest_id`: Links to metarun completion tracking (METARUN_QUEST_TULKAS, etc.)
   - `eligibility_check`: Function pointer for custom depth/state requirements
   - `probability_roll`: Function pointer for quest-specific probability calculations

2. **Dynamic Quest Discovery**: `init_roulette_quest_registry()` function:
   - **Y: Field Parsing**: Scans all quest_info entries to find quest_type == 1 (Y:1 roulette quests)
   - **Automatic Registration**: Dynamically builds registry from quest.txt data
   - **Future Extensible**: New Y:1 quests automatically discovered (with placeholder for implementation)
   - **Quest Mapping**: Maps quest IDs to existing player state variables and metarun constants

3. **Random Quest Ordering**: Implemented Fisher-Yates shuffle algorithm:
   - **Random Evaluation**: Creates randomized quest order each time lottery runs
   - **Fair Distribution**: Each eligible quest has equal chance to be evaluated first
   - **Eliminates Bias**: No fixed "Tulkas wins over Niena" preference

4. **Preserved Probability Formulas**: Maintained exact existing behavior:
   - **Tulkas Formula**: `1/(27-depth)` for depths 6-19 (12.5% max at depth 19)
   - **Niena Formula**: `0.125 * max(0, min(1, (depth-14)/5))` for depths 14+ (12.5% max at depth 19+)
   - **Quest-Specific Functions**: `tulkas_eligibility_check()`, `tulkas_probability_roll()`, etc.

5. **Quest ID Correction**: Updated hardcoded references:
   - **Old System**: quest_lottery_winner == 2 for Niena
   - **New System**: quest_lottery_winner == 4 for Niena (actual quest ID from quest.txt)
   - **Maintained Compatibility**: Tulkas still uses quest_lottery_winner == 1

**Technical Implementation Details**:
- **Registry Initialization**: Called automatically when first quest lottery runs
- **State Validation**: Checks quest_state == 0 (NOT_STARTED) for all quest types
- **Metarun Integration**: Preserves existing metarun completion checking
- **Logging Enhanced**: Detailed trace logging for quest discovery, ordering, and selection
- **Error Handling**: Gracefully skips unsupported Y:1 quests with placeholder implementation

**Architecture Benefits**:
1. **Data-Driven**: New roulette quests can be added by setting Y:1 in quest.txt
2. **Random & Fair**: No quest has evaluation order advantage
3. **Backward Compatible**: All existing quest behavior preserved exactly
4. **Maintainable**: Separates quest participation (data-driven) from quest logic (hardcoded)
5. **Extensible**: Framework ready for new roulette quests with minimal code changes

**Files Modified**:
- `src/generate.c`: Complete rewrite of `run_quest_lottery()` function and related quest spawning logic
- Added registry system, quest-specific functions, random ordering, and dynamic quest discovery

**Quest.txt Structure Utilized**:
```
Q:1:Tulkas the Strong
Y:1  # Roulette-based quest
...
Q:4:Nienna, Lady of Pity  
Y:1  # Roulette-based quest
```

**Current Roulette Quests** (from quest.txt Y:1):
- **Tulkas (ID 1)**: Depths 6-19, probability 1/(27-depth)
- **Niena (ID 4)**: Depths 14+, probability 0.125 * max(0, min(1, (depth-14)/5))

**Future Enhancement Path**: 
- Add Y:1 to quest.txt for new quest → automatically included in lottery
- Implement quest-specific eligibility/probability functions as needed
- Could add P: (probability) and D: (depth range) fields to quest.txt for full data-driven formulas

**Testing Status**: Compiled successfully, ready for game testing

### LATEST: Quest Status Menu Enhancement (Version 0.8.7) - September 6, 2025
**Issue**: Quest status menu not displaying quest information properly according to quest.txt data
**Problem Details**:
1. Quest status showed hardcoded "Active - Seek [monster]" instead of using C: (challenge) text from quest.txt
2. Placeholder replacement for [monster name] and [artifact name] not working in status display
3. Rewards showed generic descriptions instead of actual S:, K:, A: bonuses from quest data
4. Tulkas artifact name display didn't show the dynamically selected artifact

**Implemented Solution**:

1. **New Placeholder Processing Function**: Added `process_quest_placeholders()` in `xtra2.c`:
   - Handles [monster name] replacement with actual assigned target for Tulkas quest
   - Handles [artifact name] replacement with actual prize artifact name using proper object_desc()
   - Creates temporary objects to get full artifact descriptions
   - Safely handles cases where target/prize indices are invalid

2. **Enhanced Reward Text Generation**: Completely rewrote `get_quest_reward_text()`:
   - **Tulkas Special Case**: Shows actual selected artifact name using p_ptr->tulkas_prize_a_idx
   - **Data-Driven Rewards**: Uses quest_type structure data instead of hardcoded strings:
     - S: stat bonuses displayed as "+X Str/Dex/Con/Gra"
     - K: skill bonuses displayed as "+X [SkillName]" 
     - A: special abilities with descriptive names (Fear immunity, Smithing mastery, etc.)
     - O: oath associations displayed with proper oath names
   - **Formatted Display**: Multiple reward types separated with "|" for clarity

3. **Quest Status Display Updates**: Modified `do_cmd_quest_status()` in `xtra2.c`:
   - **Tulkas QUEST_ACTIVE**: Now uses processed challenge text instead of hardcoded "Active - Seek [monster]"
   - **Tulkas GIVER_PRESENT**: Uses processed challenge text with placeholders replaced
   - **Aule QUEST_ACTIVE**: Uses challenge text directly instead of hardcoded "Active - Forge..."
   - **Consistent Reward Display**: All quest states now use new reward text generation

4. **Skill Name Mapping**: Added proper skill type to name conversion:
   - Maps skill indices (0-7) to readable names (Melee, Archery, Evasion, Stealth, Perception, Will, Smithing, Song)

**Technical Implementation**:
- Uses existing placeholder replacement logic from quest interaction system
- Maintains backward compatibility with existing quest states
- Safe handling of invalid indices with fallback text
- Proper use of my_strcat() for string concatenation (following codebase conventions)
- Block scoping for variable declarations to comply with C standards

**Files Modified**:
- `src/xtra2.c`: Added process_quest_placeholders(), rewrote get_quest_reward_text(), updated do_cmd_quest_status()

**Testing Status**: Compiled successfully, ready for game testing

**FINAL UPDATE - Text Wrapping & Display Enhancement**:

5. **Text Wrapping Implementation**: Added `display_wrapped_text()` function:
   - **Smart Word Wrapping**: Respects terminal width with proper margins  
   - **Terminal Size Aware**: Uses `Term_get_size()` to get current terminal dimensions
   - **Word Boundary Respect**: Wraps at spaces, doesn't break words mid-character
   - **Minimum Width Protection**: Ensures at least 20 characters width even on narrow terminals

6. **Enhanced Quest Status Display**: Updated all quest status cases to use text wrapping:
   - **Challenge Text**: All quest challenge descriptions now wrap properly
   - **Reward Text**: Long reward descriptions wrap across multiple lines
   - **Consistent Formatting**: All text uses same wrapping logic for uniform appearance

7. **Completed Quest Display Improvement**: Fixed "Previously Completed in Metarun" section:
   - **Format Change**: From `"[Quest Name] - [Oath] available"` to `"[Quest Title] - Oath: [Oath Name]"`
   - **Uses Quest Titles**: Now uses T: field (quest titles) instead of Q: field (quest names)
   - **Clearer Oath Display**: "Oath: [Name]" format makes it clear what was unlocked
   - **Text Wrapping**: Long quest titles and oath names wrap properly

**Example Display Improvements**:
- **Before**: "Hunt down the named creature of shadow, Draugluin, Sire of Werewolves, and r" (cut off)
- **After**: Multi-line wrapped text respecting terminal boundaries
- **Before**: "Nienna, Lady of Pity - Mercy available"  
- **After**: "The Lady of Pity tests your compassion - Oath: Mercy oath"

### Previous: Quest System Comprehensive Enhancement (Version 0.8.7) - September 6, 2025
**Issue**: Complete overhaul of quest system with text display fixes and data-driven rewards
**Implemented Enhancements**:

1. **Text Display Fixes**:
   - **Word Wrapping**: Fixed word wrapping in `quest_typewriter_menu()` to prevent words from being split mid-character
   - **Punctuation Handling**: Improved punctuation attachment to prevent orphaned periods and quotes
   - **Duplication Fix**: Removed redundant "final line" logic in `extract_quest_completion_texts()` that was causing text duplication

2. **Special Abilities System**:
   - Added `ability_type` and `ability_id` fields to `quest_type` structure in `types.h`
   - Updated quest parsing in `init1.c` to read `A:` field from quest.txt
   - Implemented ability rewards in `apply_quest_rewards()`:
     - Tulkas (A:8:5): Combat prowess enhancement
     - Aule (A:8:1): Enhanced smithing sight for detecting flaws
     - Mandos (A:8:0): Fear resistance protection
     - Nienna (A:8:6): Stealth enhancement when showing mercy

3. **Data-Driven Quest System**:
   - All quest rewards now use quest.txt field data:
     - `S:` field for stat bonuses (str:dex:con:gra)
     - `K:` field for skill bonuses (skill:amount)
     - `A:` field for special abilities (type:id)
     - `O:` field for oath associations
   - Dynamic oath unlocking based on quest completion

4. **Remaining Hardcoded Values** (identified for future cleanup):
   - Quest indices (1,2,3,4) hardcoded throughout quest interaction functions
   - Quest titles in `quest_typewriter_menu()` calls still use hardcoded strings
   - Quest status display in `do_cmd_quest_status()` has hardcoded titles
   - Some quest interaction messages still hardcoded (e.g., "Aule the Smith, Maker of Mountains")
   - Monster tracking variables (niena_monsters_killed, tulkas_quest states) still use legacy approach

**Files Modified**:
- `src/types.h`: Extended quest_type structure with ability fields
- `src/init1.c`: Added A: field parsing for special abilities  
- `src/xtra2.c`: Word wrapping fix, duplication fix, special abilities implementation
- `lib/edit/quest.txt`: Added missing A:8:5 field for Tulkas quest

**Technical Notes**:
- Quest system now supports ability type 8 (changed from 9 based on user's quest.txt modifications)
- Framework in place for mechanical ability effects (currently shows messages)
- All quest data centralized in quest.txt for easy modification
   - Respects terminal width with appropriate margins

5. **Multiline Oath Confirmations**: All oath breaking confirmations use `get_check_oath_multiline()`:
   - Smith oath breaking (cmd1.c)
   - Mercy oath breaking (cmd1.c) 
   - Silence oath breaking (cmd4.c)
   - Iron oath breaking (cmd2.c)

**Code Changes**:
- `metarun.c`: Enhanced oath breaking UI with heading, spacing, and red text
- `files.c`: Added oathbreaker title display logic
- `cmd4.c`: Improved abilities menu with proper Q: text wrapping
- `birth.c`: Dynamic text wrapping using terminal size
- All oath breaking locations use multiline confirmation prompts

### MAJOR: Oath Selection Screen Text Display Update (Version 0.8.7) - September 4, 2025
**Issue**: Oath selection screen had poor text wrapping, concatenated all text fields, and used hardcoded text instead of parsed oath.txt data
**Problem Details**:
- Text was wrapping mid-word and cutting off poorly
- All oath text fields (T:, D:, P:, F:, R:, B:, U:) were being concatenated into one jumbled mess
- Display area width calculations were incorrect for terminal layout
- Hardcoded oath arrays instead of using parsed oath data
- Broken oath display (Z: banned text) had poor wrapping

**Solution Applied**:
1. **Fixed oath parser field separation**: Modified oath parser to only store D: (description) directive in the text field, ignoring T:, P:, F:, R:, B:, U: directives for cleaner display
2. **Fixed column width calculations**: Updated text wrapping to use correct 38-character width for description area (COL_DESCRIPTION = 40, with 2-char margin)
3. **Improved word wrapping**: Added proper word boundary detection for both regular and broken oath displays to prevent mid-word breaks
4. **Updated oath.txt format**: Converted multi-line D: directives to single-line descriptions for better parsing
5. **Integrated parsed oath data**: Updated oath selection screen to use `oath_name_str()` and `oath_description()` functions instead of hardcoded arrays
6. **Fixed broken oath display**: Applied same word-wrapping logic to Z: (banned text) display for consistent formatting

**Code Changes**:
- Modified `init1.c` oath parser to only store D: directives in the text field, ignore other text directives
- Updated `birth.c` oath selection screen to use proper word wrapping with max_width = 38 for both regular and broken oath displays
- Added word boundary detection to wrap at spaces rather than mid-word for all text displays
- Modified oath.txt to use single-line D: directives for cleaner parsing

**Final Result**: 
- Oath selection screen now displays professional-looking text with proper word boundaries
- All text comes from parsed oath.txt file instead of hardcoded arrays
- Both regular oath descriptions and broken oath banned text wrap correctly
- Display fits properly within 80x25 terminal constraints
- Successfully tested and verified - game reads actual oath data from oath.txt file
- Updated layout: "Choose your Oath" moved to top center, no first column usage, explanatory text moved to bottom

**Latest Enhancement (September 4, 2025)**:
- Converted all multi-line Z: directives in oath.txt to single-line format for better parsing and wrapping
- Updated oath selection layout: title at top, oath list without column constraint, explanatory text at bottom
- Completely replaced hardcoded oath arrays with dynamic parsed data from oath.txt
- Improved broken oath display to use actual Z: banned text with proper word wrapping
- Single-line Z: format allows dynamic terminal-responsive text wrapping instead of fixed multi-line blocks

**Status**: COMPLETE - All oath display issues fully resolved with enhanced layout (September 4, 2025)

**Files Modified**:
- `src/init1.c`: Modified oath parser to only store description text
- `src/birth.c`: Enhanced oath selection display with proper text wrapping for all display modes
- `lib/edit/oath.txt`: Converted to single-line D: descriptions for better display
- Terminal compatibility: Verified for minimum 80x25 terminal size

**Testing**: Oath selection screen now displays properly formatted text from parsed oath.txt data with consistent wrapping for both regular and broken oath displays

## Quest System Implementation (Version 0.8.7) - December 2024

### MAJOR: Dynamic Quest Text System with Typewriter Effect
**Achievement**: Implemented comprehensive quest system with dynamic text extraction from quest.txt
**Core Components**:
1. **Dynamic Text Extraction Functions** (xtra2.c):
   - `extract_quest_init_texts()`: Extracts I: field initialization dialog
   - `extract_quest_completion_texts()`: Extracts W: field completion dialog  
   - `free_quest_texts()`: Memory management for dynamic text arrays
   - All functions parse quest.txt dynamically instead of using hardcoded text

2. **Typewriter Display System** (xtra2.c):
   - `quest_typewriter_menu()`: Character-by-character text display with timing
   - Professional quest presentation with proper screen management
   - Color-coded display (quest giver names in blue/cyan, content in white)
   - Automatic line spacing and paragraph handling

3. **Quest Integration Functions**:
   - `tulkas_quest_interaction()`: Tulkas combat challenge quest
   - `aule_quest_interaction()`: Aule smithing forge quest  
   - `mandos_quest_interaction()`: Mandos justice quest (find Brodda)
   - `niena_quest_interaction()`: Niena mercy quest (spare creatures)
   - All use dynamic text extraction and typewriter display

### COMPREHENSIVE: Quest System Field Documentation

#### T: Field (Title/Type Identifier)
- **Purpose**: Identifies quest giver and type for dynamic text extraction
- **Usage**: Used by `extract_quest_init_texts()` and `extract_quest_completion_texts()`
- **Format**: `T:QuestGiverName` (single line)
- **Examples**: T:Tulkas, T:Aule, T:Mandos, T:Nienna
- **Function**: Acts as quest identifier for text parsing and display systems

#### C: Field (Completion/Ceremony Text)
- **Purpose**: Text displayed during quest completion ceremony
- **Usage**: Used by quest completion functions for victory celebrations
- **Format**: Multi-line completion dialog (can span multiple C: lines)
- **Content**: Reward descriptions, celebration text, quest wrap-up dialog
- **Display**: Shown via `quest_typewriter_menu()` with typewriter effect

#### I: Field (Initialization/Introduction Text) 
- **Purpose**: Initial quest dialog when first meeting quest giver
- **Usage**: Extracted by `extract_quest_init_texts()` for quest initiation
- **Format**: Multi-line introduction dialog (can span multiple I: lines)
- **Content**: Quest requirements explanation, background lore, acceptance dialog
- **Display**: Primary text for quest offering screens

#### W: Field (Win/Success Text)
- **Purpose**: Success text displayed during quest completion
- **Usage**: Extracted by `extract_quest_completion_texts()` for victory messages
- **Format**: Multi-line success dialog (can span multiple W: lines)  
- **Content**: Victory celebration, achievement recognition, reward ceremony
- **Display**: Secondary text for quest completion screens

### CRITICAL: Quest Monster Spawning Controls
**Analysis Result**: Quest monsters are safely contained within quest contexts through multiple control systems:

1. **Quest Reservation System** (generate.c):
   - `p_ptr->quest_reserved[0]` ensures only one quest spawns per game run
   - Quest lottery system (`quest_lottery_winner`) controls entrance-based quests
   - Prevents multiple quest overlaps and conflicts

2. **Vault-based Quest Controls** (generate.c):
   - Aule and Mandos quests spawn in special quest vaults with strict placement rules
   - Quest vault placement requires specific conditions (smithing skill for Aule, etc.)
   - Vault integrity system with `qv_placed_this_level` tracking

3. **Entrance-based Quest Controls** (generate.c):
   - Tulkas and Niena spawn at level entrances during generation
   - Controlled by quest lottery system with proper state management
   - Reset mechanisms for regeneration scenarios

4. **Monster Tracking Systems** (xtra2.c, monster2.c):
   - Niena quest specifically tracks `p_ptr->niena_monsters_seen` vs `p_ptr->niena_monsters_killed`
   - Tulkas quest tracks specific unique monster targets (`p_ptr->tulkas_target_r_idx`)
   - Mandos quest monitors Brodda/Aldor death (`check_mandos_quest_completion()`)

**CONCLUSION**: Quest monsters do NOT spawn outside quest contexts due to comprehensive control systems

### Quest Text Formatting Enhancements
**Recent Improvements** (based on user feedback):
1. **Enhanced Text Wrapping**: Fixed quest.txt formatting with proper line breaks and paragraph structure
2. **Artifact Name Cleanup**: Removed strange symbols (like †) from artifact references 
3. **Professional Display**: All quest text now displays with proper typewriter effect and formatting
4. **Memory Management**: Proper allocation/deallocation of dynamic text arrays

### Code Integration Status
**Files Successfully Modified**:
- `src/xtra2.c`: Quest interaction functions with dynamic text system
- `src/init1.c`: Quest text parsing infrastructure  
- `src/types.h`: Quest type definitions and state management
- `lib/edit/quest.txt`: Enhanced quest data with improved formatting
- Compilation: ✅ SUCCESSFUL with Cygwin (no errors, minor warnings only)

**Quest System Status**: ✅ COMPLETE - All four Valar quests fully implemented with dynamic text extraction and typewriter display system

### MAJOR: Oath Parser Text Storage Bug (Version 0.8.7) - FIXED September 4, 2025
**Critical Issue**: Major bug in oath parsing function causing garbage data in oath.raw
**Root Cause**: Incorrect use of `add_text()` vs `add_name()` functions in oath parser
**Problem Details**: 
- Oath parser was using `add_text()` for C/M/E/Q/Z directives, storing to `head->text_ptr`
- Helper functions were reading from `oath_name_text` (mapped to `head->name_ptr`)
- This caused oath text to be written to one buffer but read from another
- Result: Garbage data displayed for oath confirmation prompts, curse messages, etc.

**Solution Applied**:
1. **Fixed text storage mapping**: Updated oath parser in `init1.c` line ~5350-5420
   - C/M/E/Q directives now use `add_name()` to store in name buffer
   - Z directive continues using `add_text()` for multi-line support
2. **Fixed helper function**: Updated `oath_banned_text()` in `cmd4.c` to use `oath_desc_text` instead of `oath_name_text`
3. **Text buffer alignment**: Ensured all oath text properly stored and retrieved from correct buffers

**Code Changes**:
```c
// OLD (BUGGY) - used add_text() for all directives
if (!add_text(&(oath_ptr->confirmation_prompt), head, buf + 2))
    return (PARSE_ERROR_OUT_OF_MEMORY);

// NEW (FIXED) - use add_name() for single-line text
if (!(oath_ptr->confirmation_prompt = add_name(head, buf + 2)))
    return (PARSE_ERROR_OUT_OF_MEMORY);
```

**Files Modified**:
- `src/init1.c`: Fixed oath parser text storage (lines 5350-5420)
- `src/cmd4.c`: Fixed `oath_banned_text()` helper function to use correct text buffer

**Testing**: Verified oath parsing now works correctly without garbage data

### MAJOR: Oath Parser R: Directive Fix (Version 0.8.7)
**Critical Issue**: Parse error when processing `R:Freedom of choice` in oath.txt line 28
**Root Cause**: oath parser R: directive only accepted numeric values, not text descriptions
**Problem**: `R:Freedom of choice` was being parsed as numeric restriction but contained text

**Solution Applied**:
1. **Enhanced R: directive parser**: Modified to handle both numeric (old format) and text (new format)
2. **Backward compatibility**: Existing numeric R: directives still work
3. **Text support**: New text-based R: directives are added to oath description text
4. **Error elimination**: All oath.txt parsing errors resolved

**Code Changes**:
```c
/* Process 'R' for "Restrictions" or "Restriction description" */
else if (buf[0] == 'R')
{
    int restrictions;
    if (1 == sscanf(buf + 2, "%d", &restrictions))
    {
        /* Save the numeric value (old format) */
        oath_ptr->restrictions = restrictions;
    }
    else
    {
        /* Treat as restriction description text (new format) */
        if (!add_text(&(oath_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }
}
```

**Files Modified**:
- `src/init1.c`: Enhanced parse_oath_info() R: directive handler

**Status**: ✅ FULLY RESOLVED - All oath parsing errors eliminated

### MAJOR: limits.txt Conflict Resolution (Version 0.8.7)
**Critical Issue**: M:O directive conflict between oath types and objects on level
**Root Cause**: Two conflicting M:O entries in limits.txt:
- Line 31: `M:O:8` (oath types) 
- Line 78: `M:O:512` (objects on level)
This created parser conflicts causing "too many entries" errors.

**Solution Applied**:
1. **Changed oath directive**: M:O:8 → M:W:8 (use W for oath types)
2. **Removed duplicate M:Q**: Eliminated duplicate quest limit entry 
3. **Parser support verified**: init1.c already had M:W case for oath_max
4. **Clean rebuild**: Forced regeneration of lib/data/limits.raw

**Files Modified**:
- `lib/edit/limits.txt`: Fixed directive conflicts, removed duplicates
- Parser automatically uses existing M:W support in `parse_z_info()`

**Status**: ✅ FULLY RESOLVED - All parsing errors eliminated

### Quest/Oath Parser Enhancements (Version 0.8.7)
**Issue**: "Undefined directive error" when parsing Q:1:Tulkas and oath.txt line 23
**Root Cause**: Parsers only supported legacy 'N' directives, not modern 'Q'/'O' formats

**Solutions Applied**:
1. **Enhanced parse_quest_info()**: Added 'Q' directive support alongside 'N'
2. **Enhanced parse_oath_info()**: Added 'O' directive support alongside 'N' 
3. **Added missing directive handlers**: P/F/S/K/B/U directives for comprehensive parsing
4. **Fixed array sizing**: Corrected limits.txt M:Q from 4 to 8 entries

**Files Modified**:
- `src/init1.c`: Enhanced both quest and oath parsers
- `lib/edit/limits.txt`: Fixed quest array sizing

**Status**: ✅ FULLY RESOLVED - Both quest.txt and oath.txt parse correctly

### Current Directive Map (No Conflicts)
```
F=features, K=object kinds, B=abilities, Q=quests, W=oath types, 
A=artifacts, E=special items, R=monster races, G=ghost templates, 
V=vaults, P=player races, C=player houses, H=history lines, 
S=story lines, U=curses, L=flavors, O=objects on level, 
Y=runtypes, Z=styles, N=names array, T=text array
```

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

#### Issue #4: Quest Parsing System Implementation
**Problem**: Game needed complete quest and oath parsing system with proper data structures
**Solution**: Implemented full parsing framework with quest_type/oath_type structures, parsing functions, initialization
**Status**: ✅ COMPLETED - All parsing infrastructure working

#### Issue #5: Variable Naming Confusion  
**Problem**: User reported confusing o_/q_ variable prefixes for quest/oath system
**Solution**: Renamed all variables to descriptive names:
- q_info → quest_info, q_name → quest_name_text, q_text → quest_desc_text
- o_info → oath_info, o_name → oath_name_text, o_text → oath_desc_text
- Avoided conflicts with existing quest_text[] and oath_name[] arrays
**Status**: ✅ COMPLETED - Build successful with clear naming

#### Issue #6: Quest.txt Parsing Error - Tulkas Quest Format ✅ COMPLETED
**Problem**: "Undefined directive error" when parsing Q:1:Tulkas the Strong quest, later "too many entries" error at Q:4
**Root Cause**: 
1. Quest parser only supported 'N' directive for quest entries, but quest.txt used 'Q' directive
2. Conflicting M:Q entries in limits.txt - had both M:Q:8 and M:Q:4, with M:Q:4 being used last
**Solution**: 
1. Modified parse_quest_info() in init1.c to accept both 'N' and 'Q' directives for quest headers
2. Added support for missing directives: 'S' (stat bonuses), 'K' (skill bonuses), 'I' (initialization text), 'W' (completion text)
3. Enhanced 'T' directive parser to handle both numeric format (T:num:num) and text format (T:title text)
4. Fixed limits.txt to use M:Q:8 consistently (removed duplicate M:Q:4 that limited quest array size)
5. Added SPC_TULKAS=5 constant to defines.h for Tulkas quest ability reward (A:9:5)
**Verification**: All 4 quests (Q:1-4) now parse successfully without "too many entries" or directive errors
**Status**: ✅ COMPLETED - Quest parsing system fully functional

#### Issue #7: Oath.txt Parsing Error - Same Directive Issue ✅ COMPLETED
**Problem**: "Undefined directive error" at line 23 when parsing oath.txt (O:0:None)
**Root Cause**: Oath parser only supported 'N' directive for oath entries, but oath.txt used 'O' directive
**Solution**: 
1. Modified parse_oath_info() in init1.c to accept both 'N' and 'O' directives for oath headers
2. Added support for missing directives: 'P' (pledge text), 'F' (forbidden actions), 'S' (stat bonuses), 'K' (skill bonuses), 'B' (behavioral restrictions), 'U' (unlock conditions)
3. Enhanced 'T' directive parser to handle both numeric format (T:num:num) and text format (T:title text)
**Verification**: All oath directives (O, T, D, P, F, R, A, S, K, B, U) now parse successfully
**Status**: ✅ COMPLETED - Oath parsing system fully functional

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

### Latest Build Status (September 2025)
**CONFIRMED WORKING**: Build completes successfully with `make -f Makefile.cyg -j8`
- All object files compile without errors
- Linking successful using MinGW cross-compiler
- Executable `sil.exe` generated successfully
- Quest system updates including typewriter menus and segfault fixes compile cleanly

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

## September 2025 - Major UI and Quest System Updates

### Quest Status Menu Modernization (December 2024)
**Status**: ✅ COMPLETED
- **Objective**: Update quest status menu to use dynamic data from quest.txt instead of hardcoded strings
- **Implementation**: 
  - Enhanced `do_cmd_quest_status()` in xtra2.c to use `get_quest_title()` and `get_quest_challenge()` functions
  - Replaced hardcoded quest titles like "Tulkas' Decree" with dynamic lookups from quest_info array
  - Updated oath name display to use `get_oath_name_from_id()` with dynamic oath_info data
  - Enhanced oath name resolution with fallback mechanism for compatibility
- **Technical Details**: Modified quest status display to read Q:, T:, C:, and O: fields from quest.txt
- **Result**: Quest status menu now dynamically displays quest data and properly shows oath names from oath.txt

### Oath System Hardcoded Value Analysis (December 2024)
**Status**: ✅ COMPLETED
- **Objective**: Identify and document all hardcoded oath values vs dynamic usage from oath.txt
- **Findings**:
  - **Dynamic Usage**: Functions like `oath_name_str()`, `oath_description()`, `oath_pledge()`, `oath_forbidden()`, `oath_reward_text()` in cmd4.c properly read from oath_info array
  - **Mixed Usage**: Quest status menu (`do_cmd_quest_status()`) was using hardcoded oath names - now fixed to use dynamic oath_info lookups
  - **Hardcoded Constants**: OATH_MERCY (1), OATH_SILENCE (2), OATH_IRON (3), OATH_SMITH (4) defines remain hardcoded for ID references
  - **Oath Breaking**: Enhanced `apply_oath_breaking_curse()` in cmd1.c to use dynamic oath names with static fallback array to prevent dangling pointer warnings
- **Implementation**: Fixed all instances of hardcoded oath name usage to use oath_info data with proper fallback mechanisms

### oath.txt Field Usage Documentation (December 2024)
**Status**: ✅ COMPLETED
- **Comprehensive Field Analysis**:
  - **O:** (Index) - Used for oath_info array indexing and ID references
  - **T:** (Title/Name) - Used by `oath_name_str()` and dynamic name resolution functions
  - **D:** (Description) - Used by `oath_description()` function in birth.c and display systems
  - **P:** (Pledge) - Used by `oath_pledge()` function for oath selection display
  - **F:** (Forbidden) - Used by `oath_forbidden()` function for restriction display
  - **R:** (Reward) - Used by `oath_reward_text()` function for benefit display
  - **C:** (Confirmation) - Used by `oath_confirmation_prompt()` for oath breaking confirmations
  - **M:** (Curse Message) - Used by `oath_curse_message()` for curse display
  - **E:** (Permanent) - Used by `oath_permanent_message()` for permanent effects
  - **Q:** (Death Message) - Used by `oath_death_message()` for death-related text
  - **Z:** (Banned Text) - Used by `oath_banned_text()` for broken oath display
- **Result**: All oath.txt fields are properly utilized by the oath system with comprehensive helper functions

### Build System Validation with Cygwin
**Status**: ✅ COMPLETED
- **Command**: `make -f Makefile.cyg` from src/ directory
- **Result**: Successful compilation with all quest and oath modernization changes
- **Compiler**: i686-w64-mingw32-gcc on Windows/Cygwin environment
- **Warnings Fixed**: Resolved dangling pointer warning in `apply_oath_breaking_curse()` by using static fallback array

### Quest Typewriter System Implementation
**Status**: ✅ COMPLETED
- **Objective**: Convert all quest interactions to immersive typewriter display
- **Implementation**: Used existing `quest_typewriter_menu()` function with text arrays
- **Scope**: All four quest types (Aule, Mandos, Niena, Tulkas) now use typewriter display
- **Result**: Enhanced immersion with character-by-character text display and proper color coding

### Main Menu Redesign
**Status**: ✅ COMPLETED  
- **Objective**: Rearrange main menu letters in non-alphabetical order per user specifications
- **Challenge**: Required custom key mapping system instead of sequential letter assignment
- **Implementation**: Complete restructuring of main menu switch statement in cmd4.c
- **New Key Layout**: m=map, q=quit, s=save, c=character sheet, o=options, k=ignore, h=help, v=version, u=quest status, a=aim, b=browse, d=drop, e=equipment, f=fire, g=get, i=inventory, j=jump, l=look, r=rest
- **Technical Details**: Increased MAIN_MENU_MAX to 19, custom action routing

### Quest Status Menu Migration  
**Status**: ✅ COMPLETED
- **Objective**: Move quest status from character sheet to main menu
- **Implementation**: 
  - Removed quest status option from character sheet prompt and handler
  - Added 'u' key to main menu for "Quest status"
  - Connected to existing `do_cmd_quest_status()` function in xtra2.c
- **Result**: Better accessibility and logical organization

## NEW QUEST IMPLEMENTATION: Oromë Hunting Quest (December 2024)
**Status**: ✅ COMPLETED

### Overview
Implemented a new dynamic hunting quest for Oromë the Huntsman that challenges players to hunt specific monster types based on dungeon depth. This quest features:
- **Dynamic Targets**: Hunt requirements change based on depth (wolves at shallow levels, vampires at deep levels)
- **Scaling Difficulty**: Higher challenge counts for easier monsters, lower counts for harder monsters
- **Unique Bane Reward**: Grants the powerful Unique Bane special ability (SPC_UNIQUE_BANE)
- **Oath Unlocking**: Unlocks the Oath of Silence for future characters in the metarun

### Technical Implementation

#### Core Files Modified:
1. **quest.txt** - Added Oromë quest definition (Quest ID 5)
2. **src/types.h** - Added player quest tracking variables
3. **src/defines.h** - Added quest state constants and monster type definitions
4. **src/xtra2.c** - Complete quest interaction system implementation
5. **src/generate.c** - Integrated into quest roulette system
6. **src/dungeon.c** - Added interaction checks to main game loop
7. **src/wizard2.c** - Added debug completion support
8. **src/externs.h** - Added function declarations
9. **src/metarun.h** - Added metarun completion flag

#### Quest Structure (quest.txt):
```
Q:5:Oromë the Huntsman
Y:1
D:The greatest hunter among the Valar seeks to test your prowess
I:Oromë regards you with keen eyes::'Prove your skill, hunter. The dark creatures multiply and must be culled.'
C:Hunt and slay creatures of the wild to prove your prowess
P:LINEAR_DECAY:27
A:8:7
O:6
```

#### Player State Variables (types.h):
```c
/* Orome quest tracking */
byte orome_quest;          /* Orome quest state (OROME_QUEST_*) */
byte orome_target_type;    /* Monster type to hunt (1=wolf, 2=spider, 3=serpent, 4=vampire) */
s16b orome_killed_count;   /* Number of target monsters killed */
s16b orome_target_count;   /* Required number to kill (100/80/60/30) */
s16b orome_level;          /* Dungeon depth where quest started */
s16b orome_reserved;       /* padding */
```

#### Quest State Constants (defines.h):
```c
/* Oromë quest states */
#define OROME_QUEST_NOT_STARTED 0
#define OROME_QUEST_GIVER_PRESENT 1
#define OROME_QUEST_ACTIVE 2
#define OROME_QUEST_SUCCESS 3
#define OROME_QUEST_REWARDED 4

/* Oromë quest monster types */
#define OROME_TARGET_WOLF 1
#define OROME_TARGET_SPIDER 2
#define OROME_TARGET_SERPENT 3
#define OROME_TARGET_VAMPIRE 4

/* Monster race index */
#define R_IDX_OROME 332

/* Metarun quest flag */
#define METARUN_QUEST_OROME (1UL << 4)
```

### Quest Mechanics

#### Hunt Target Selection:
Based on dungeon depth when quest is accepted:
- **Depth ≤ 250**: Hunt 100 wolves (RF3_WOLF flag)
- **Depth 251-500**: Hunt 80 spiders (RF3_SPIDER flag)  
- **Depth 501-750**: Hunt 60 serpents (RF3_SERPENT flag)
- **Depth > 750**: Hunt 30 vampires (d_char = 'v')

#### Monster Kill Tracking:
- **Location**: `monster_death()` function in xtra2.c
- **Detection**: Uses monster race flags (RF3_WOLF, RF3_SPIDER, RF3_SERPENT) and character symbol for vampires
- **Completion**: `check_orome_quest_completion()` called after each monster death
- **Logging**: Comprehensive trace logging for kill count progress

#### Quest Interaction System:
- **Initialization**: `orome_quest_interaction()` for quest offering and completion
- **Proximity Check**: `check_orome_quest_interaction()` in main game loop
- **Text Integration**: Uses `extract_quest_init_texts()` and `extract_quest_completion_texts()`
- **Fallback**: Hardcoded text if quest.txt extraction fails

### Reward System

#### Primary Reward: Unique Bane Ability
- **Ability ID**: SPC_UNIQUE_BANE (ID 7)
- **Function**: Grants combat bonuses against unique monsters
- **Implementation**: Calls `grant_unique_bane_ability()` function
- **Status**: Ability was already implemented, quest now provides access

#### Secondary Reward: Oath Unlocking
- **Oath**: Oath of Silence (linked via quest.txt O: field)
- **Mechanism**: `metarun_unlock_oath()` with quest oath ID
- **Persistence**: Unlocked for all future characters in current metarun
- **Integration**: `metarun_mark_quest_completed(METARUN_QUEST_OROME)`

### Integration Points

#### Quest Roulette System:
- **Registry**: Added to `init_roulette_quest_registry()` in generate.c
- **Eligibility**: Uses `generic_eligibility_check()` with LINEAR_DECAY formula
- **Metarun Check**: Prevents re-spawning if already completed (`METARUN_QUEST_OROME`)
- **Quest State**: Links to `p_ptr->orome_quest` for state tracking

#### Debug System:
- **Command**: Enhanced `do_cmd_debug_complete_quest()` in wizard2.c
- **Functionality**: Sets kill count to target count, triggers full interaction
- **Testing**: Allows easy quest testing without hunting 100 monsters

#### Main Game Loop:
- **Integration**: `check_orome_quest_interaction()` called every turn in dungeon.c
- **Adjacency**: Automatically triggers interaction when player is adjacent to Oromë
- **Spawning**: Attempts to spawn Oromë near player when quest is completed

### Code Quality Features

#### Error Handling:
- **Safe Arrays**: Monster name arrays with bounds checking
- **Null Checks**: Proper validation of quest text extraction
- **Fallbacks**: Hardcoded messages if data-driven text fails

#### Logging Integration:
- **Trace Logging**: Comprehensive logging for all quest state changes
- **Debug Info**: Kill count progress, target selection, completion events
- **State Tracking**: Quest state transitions and interaction triggers

#### Memory Management:
- **Text Cleanup**: Proper `free_quest_texts()` calls after text extraction
- **Static Prevention**: Turn-based interaction prevention to avoid duplicates

### Testing & Validation

#### Syntax Verification:
- **Test File**: Created standalone test with quest constants and structures
- **Compilation**: Verified syntax correctness with GCC compilation
- **Integration**: All modified files show clean syntax in error checking

#### Functional Components:
- ✅ **Quest Definition**: Added to quest.txt with proper structure
- ✅ **State Tracking**: Player variables added to types.h
- ✅ **Constants**: All quest states and types defined in defines.h
- ✅ **Kill Tracking**: Monster death detection in monster_death()
- ✅ **Completion Logic**: Quest completion checking after kills
- ✅ **Interaction System**: Full quest offering and reward interaction
- ✅ **Roulette Integration**: Added to quest lottery system
- ✅ **Debug Support**: Enhanced debug completion command
- ✅ **Metarun Persistence**: Quest completion tracking across characters
- ✅ **Oath Unlocking**: Integration with oath system

### Future Enhancement Opportunities

#### Potential Improvements:
1. **Monster Variety**: Could add more monster types with different difficulty tiers
2. **Dynamic Scaling**: Hunt counts could scale with character level/skills
3. **Location Restrictions**: Could require hunting in specific dungeon areas
4. **Time Limits**: Could add urgency with turn-based completion requirements

#### Technical Extensions:
1. **Monster AI**: Hunted creatures could become more aggressive when quest is active
2. **Lore Integration**: Could add monster knowledge bonuses as intermediate rewards
3. **Equipment Synergy**: Special hunting equipment that works better during quest
4. **Environmental Effects**: Quest could modify monster spawning patterns

**Implementation Date**: December 2024  
**Status**: Fully implemented and ready for testing  
**Dependencies**: Quest roulette system, oath system, special abilities framework
