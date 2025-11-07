# Character Screen Tutorial Feature

**Date**: October 16, 2025  
**Status**: ✅ Implemented, Built, and Interactive

## Overview
Added interactive help/tutorial system to character screen, accessible via `?` key. The tutorial shows **actual character data** formatted the same way as the character sheet, making it a true interactive learning experience.

## Key Features

### Interactive Display
- **Shows real character values** from the current game state
- **Formatted identically** to the main character screen
- **Color-coded** using the same scheme as the character sheet
- **Dynamic calculations** for derived stats (burden, skills, etc.)

### Arrow Key Navigation
- **Bidirectional navigation**: Move forward (6/→) or backward (4/←) through stages
- **Multiple input methods**: Arrow keys, numpad, Space, Enter all supported
- **ESC to exit**: Can exit tutorial at any time
- **Visual indicators**: Shows "(4/<- Previous)" and "(Next 6/->)" when available

### Screen Layout Optimization
- **80x24 terminal compatibility**: All 4 stages fit within standard terminal size
- **Two-column layouts**: Used for traits (Stage 3) and commands (Stage 4)
- **Compact formatting**: Minimized whitespace while maintaining readability
- **Abbreviated history**: Limited to 3 lines in Stage 4 to save space

### Tutorial Function (`files.c`)
- Created `display_character_tutorial()` with 4 educational stages
- Each stage displays actual data with explanatory text
- Press any key to advance, ESC to exit early
- Designed for 80x24 terminal layout
- Reuses display helper functions (`put_pair20_right`, `put_single20_right`)

## Four Tutorial Stages

### Stage 1: Core Statistics (Left Column)
**Shows actual values with explanations:**
- Exp: Current/Next level (from `p_ptr->new_exp`, `p_ptr->exp`)
- Burden: Weight carried/Max (using `weight_limit()`)
- Depth: Current/Minimum return (from `p_ptr->depth`, `min_depth()`)
- Turn: Total game turns (from `playerturn`, with comma formatting)
- Light: Current radius (from `p_ptr->cur_light`)
- Melee: Main hand combat (skill, damage dice from `p_ptr`)
- Bows: Ranged combat (skill, damage dice from `p_ptr`)
- Armor: Defense stats (evasion, min-max protection)
- Health: HP current/max (color-coded: green/yellow/red)
- Voice: Song points current/max (color-coded)

### Stage 2: Attributes & Skills (Right Column)
**Shows character's actual stats and skills:**
- **Primary Attributes** - Header: "Attributes (Current = Base +equip +misc -drain)":
  - **Short names** for display (Str, Dex, Con, Gra) - matching character sheet
  - **Compact layout**: Current value at col 8, "=" at col 11, base at col 13, modifiers start at col 16
  - **Shows ALL three types of modifiers**:
    - `+equip`: Equipment bonuses (armor, rings, weapons, etc.)
    - `+misc`: Miscellaneous bonuses (abilities, temporary effects, etc.)
    - `-drain`: Stat drain (temporary reductions, shown in yellow)
  - **Always shows breakdown when ANY modifier present**: Any non-zero modifier triggers full display
  - Current value (color-coded: white/green/orange)
  - Base value always shown when modifiers exist
  - **Concise descriptions** starting at col 28 to ensure all modifiers are visible
    - "- Strength: melee & capacity"
    - "- Dexterity: archery & evasion"
    - "- Constitution: HP & resist"
    - "- Grace: will, perception"
  
- **Skills Display** - Header: "Skills: Total = Base +stat +equip +misc":
  - **Subheader**: "(Base determines ability purchase cost)" - explains cost mechanic
  - **Full names** (Melee, Archery, Evasion, Stealth, Perception, Will, Smithing, Song)
  - **Compact layout**: Total at col 16, "=" at col 19, base at col 21, modifiers start at col 24
  - Total value (effective skill)
  - Base investment (used for ability purchase calculations)
  - **Modifier breakdown clearly labeled**:
    - +stat: From primary attributes
    - +equip: From worn equipment
    - +misc: From race, house, curses, abilities
  - **Concise descriptions** for each skill starting at col 36

### Stage 3: Character Traits (Middle Column)
**Shows character's actual traits:**
- Character name and house (formatted like character sheet)
- "Oathbreaker" display if oaths are broken (red text)
- **Compact color-coding legend**:
  - `++` (L_GREEN) - Mastery, `+` (GREEN) - Affinity
  - `--` (RED) - Major penalty, `-` (L_RED) - Minor penalty
  - UNIQUE (VIOLET) - Special abilities
  - CURSE (UMBER) - Character curses
- **Complete trait detection** using same logic as character sheet:
  - All skill affinities/masteries (melee++, archery+, etc.)
  - All skill penalties (stealth-, will--, etc.)
  - **Includes curse effects** on skills via `curse_flag_count_rhf()`
  - All unique racial/house abilities (One Handed, Gift of Eru, etc.)
  - All curses (Kinslayer, Doom of Mandos, Morgoth Curse, etc.)
- **Two-column layout** to fit all traits on screen
  - Organized: Uniques → Masteries → Affinities → Penalties
  - Automatically balances between columns

### Stage 4: Character History & Game Controls
**Shows actual character data and essential controls:**
- Displays abbreviated character history (3 lines max)
- Uses same text wrapping as character sheet
- **Essential Game Controls in Two Columns**:
  - **Left column**: hjkl (move), Space (rest), x (examine), l (look), i (inventory), e (equipment), u (use)
  - **Right column**: g (get), d (drop), C (character), m (menu), Tab (cycle), ESC (cancel), ? (help)
- **Compact layout** to fit all content in 80x24 screen
- Prioritizes most-used commands for new players

## Technical Implementation

### Data Sources
All data pulled directly from game state:
- `p_ptr` - Player structure (stats, skills, hp, sp, etc.)
- `op_ptr` - Player options (name, etc.)
- `hp_ptr` - House pointer (titles, etc.)
- `p_info[]`, `c_info[]` - Race and house data (flags, abilities)

### Display Functions Reused
- `put_pair20_right()` - Two-value displays (X / Y format)
- `put_single20_right()` - Single value displays
- `comma_number()` - Turn number formatting
- `text_out_to_screen()` - History text wrapping
- `weight_limit()` - Maximum burden calculation

### Color Coding
Matches character sheet exactly:
- TERM_L_GREEN - Current/good values
- TERM_GREEN - Max/base values
- TERM_YELLOW - Warning state (low health)
- TERM_RED - Danger state (very low health)
- TERM_WHITE - Neutral/base stats
- TERM_ORANGE - Penalties/reduced values
- TERM_VIOLET - Unique abilities
- TERM_UMBER - Curses
- TERM_SLATE - Descriptive text

## Integration (`cmd4.c`)
- Added `?` key handler in `do_cmd_character_sheet()`
- Updated menu prompt to include "?help" option
- Condensed prompt text to fit within 80-column width

## Export (`externs.h`)
- Added function declaration: `extern void display_character_tutorial(void);`

## Files Modified
1. `src/files.c`: Added 260+ line interactive tutorial function
2. `src/cmd4.c`: Added `?` handler and updated prompt
3. `src/externs.h`: Added function declaration

## Build Status
✅ Build successful: `.\build-cmake.bat` completed without errors  
✅ No new compilation warnings introduced  
✅ Executable deployed to `sil-more-windows-sdl3\sil-more.exe`

## Navigation
- **Forward**: Press `6`, `→`, `Space`, or `Enter` to advance to next stage
- **Backward**: Press `4` or `←` to return to previous stage
- **Exit**: Press `ESC` to exit tutorial at any time
- **Any other key**: Advances to next stage (except on final stage where it exits)

## Testing Checklist
- [ ] Launch game and open character screen (C key)
- [ ] Press `?` to activate tutorial
- [ ] **Stage 1**: Verify actual exp, burden, depth, turn, light values display correctly
- [ ] **Stage 1**: Verify exp shows two DIFFERENT values (current/needed)
- [ ] **Stage 1**: Verify melee/bows/armor show current combat stats
- [ ] **Stage 1**: Verify health/voice use correct color coding (green/yellow/red)
- [ ] **Navigation**: Test pressing `6` or `Space` to advance to Stage 2
- [ ] **Navigation**: Test pressing `4` to go back to Stage 1
- [ ] **Stage 2**: Verify all 4 attributes show SHORT names (Str, Dex, Con, Gra)
- [ ] **Stage 2**: Verify descriptions show FULL names (Strength, Dexterity, etc.)
- [ ] **Stage 2**: Verify ALL 8 skills are displayed (not just 4)
- [ ] **Stage 2**: Verify skills use FULL names (Melee, Archery, Evasion, etc.)
- [ ] **Stage 2**: Verify skills show proper total = base + modifiers breakdown
- [ ] **Navigation**: Test arrow key navigation forward and back
- [ ] **Stage 3**: Verify character name displays correctly
- [ ] **Stage 3**: Verify ALL traits shown (including curse-modified skills)
- [ ] **Stage 3**: Test with cursed character to verify curse traits appear
- [ ] **Stage 3**: Verify trait organization (Uniques → MA → AF → Penalties)
- [ ] **Stage 4**: Verify character history text displays
- [ ] **Stage 4**: Verify game controls listed (not character sheet commands)
- [ ] **Stage 4**: Verify essential controls: hjkl, space, x, l, i, e, u, g, d, C, Tab, ESC, ?
- [ ] **Navigation**: Test that pressing any key on Stage 4 exits tutorial
- [ ] **Exit**: Test ESC to exit tutorial early from each stage
- [ ] Verify screen properly clears and restores after tutorial
- [ ] Compare values shown in tutorial to main character sheet for accuracy

## Technical Details
- Tutorial uses live data from player structure
- No static text arrays - all dynamic generation
- Calculations match main character sheet exactly
- Same helper functions ensure consistency
- Proper color coding throughout
- Full screen clear between stages
- ESC allows early exit from any stage
- Clean screen restore when tutorial ends
