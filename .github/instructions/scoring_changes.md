# Scoring System Changes

## Summary
Modified the scoring system to:
1. Add 3 points per unique monster killed to the base score
2. Change the ascending calculation to use level 20 as the maximum depth
3. Allow blessings to subtract from curses in the score multiplier calculation

## Changes Made

### 1. Store Unique Kills Count (`src/files.c`)
- Modified `create_score()` function to store unique monster kill count in the `cur_lev` field
- Added call to `unique_bane_type_killed()` to get the count at the time of score creation
- This value is now permanently captured in the highscore entry

### 2. Update Score Calculation (`src/files.c`)
- Modified `score_breakdown` struct to include `uniques_killed` field
- Updated `calculate_score_breakdown()` function to:
  - Read unique kills from `score->cur_lev` field
  - Add `3 * uniques_killed` to the base score
  - Changed ascending depth calculation from `40 - raw_cur_depth` to `20 - raw_cur_depth`
  - Store the unique kills count in the breakdown result

### 3. Blessing/Curse Net Value (`src/files.c`, `src/types.h`)
- Changed the `pts` field to store the **net curse/blessing value** (curses minus blessings)
- Curses are positive values, blessings are negative values
- Updated `calculate_score_breakdown()` to accept negative values (range: -1000 to +1000)
- This means blessings now properly **subtract** from the difficulty multiplier
- Updated documentation in `types.h` to clarify this behavior

### 4. Documentation Update (`src/types.h`)
- Updated the comment for `cur_lev` field in `high_score` struct from "Current Player Level" to "Unique monsters killed"
- Updated the comment for `pts` field to clarify it stores "Net curse/blessing value: curses(+) minus blessings(-)"
- This field was previously unused in the codebase

## Score Formula Changes

### Old Formula:
```
base = 10 * depth_down
if (silmarils > 0):
    base += 5 * (40 - raw_cur_depth)  // Ascending bonus
    base += 100 + 50*(silmarils-1) up to 3 silmarils
if (morgoth_slain):
    base += 300
if (escaped):
    base += 100

multiplier = 1000 + (3 - house_power) * 100 + curses * 25
           (curses clamped to 0-1000, ignoring blessings)
```

### New Formula:
```
base = 10 * depth_down
base += 3 * uniques_killed            // NEW: 3 points per unique
if (silmarils > 0):
    base += 5 * (20 - raw_cur_depth)  // CHANGED: Max level from 40 to 20
    base += 100 + 50*(silmarils-1) up to 3 silmarils
if (morgoth_slain):
    base += 300
if (escaped):
    base += 100

multiplier = 1000 + (3 - house_power) * 100 + net_value * 25
           (net_value = curses - blessings, range -1000 to +1000)
           // CHANGED: Blessings now subtract from multiplier
```

## Technical Details

- **Field Repurposed**: `cur_lev[4]` in `high_score` struct (was never used)
- **Data Range**: Clamped to 0-999 uniques (fits in 3-digit field with null terminator)
- **Blessing/Curse Storage**: The `pts[5]` field stores a signed value (can be negative)
- **Backward Compatibility**: Old scores will read 0 for unique kills (safe default)
- **Existing Function Used**: `unique_bane_type_killed()` from `cmd4.c` (already declared in `externs.h`)
- **Curse System**: `CURSE_GET()` returns signed int8_t values where negative = blessings, positive = curses

## Impact

- Players who kill more unique monsters will get bonus points (3 per unique)
- The ascending bonus (when escaping with silmarils) now correctly calculates from level 20 as the maximum
- Scoring is more granular and rewards exploration/combat beyond just depth progression
- **Blessings now make the game easier** (negative multiplier contribution) while **curses make it harder** (positive multiplier contribution)
- A character with 4 blessings and 0 curses will have a multiplier penalty of -100 basis points compared to baseline
- A character with 4 curses and 0 blessings will have a multiplier bonus of +100 basis points compared to baseline
