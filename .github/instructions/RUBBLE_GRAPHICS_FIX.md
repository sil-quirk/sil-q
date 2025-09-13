# Rubble Graphics Fix

## Problem Description
Rubble in graphics mode was displaying as a standard wall tile instead of the desired tile [0,21] from the graf-new.prf preference file.

## Root Cause Analysis
The issue was caused by the DEPTH_BASED_WALLS system in `cave.c` that overrides preference file graphics for wall features. 

### Technical Details:
1. **FEAT_RUBBLE** (49) falls within the wall feature range (**FEAT_WALL_HEAD** to **FEAT_WALL_TAIL**: 48-63)
2. The graf-new.prf file correctly contains: `F:49:0x80/0x95` (tile [0,21])
3. However, the condition on line 1527 of `cave.c` was applying depth-based wall graphics to ALL wall features:
   ```c
   if (!graphics_are_ascii() && (feat >= FEAT_WALL_HEAD && feat <= FEAT_WALL_TAIL))
   ```
4. This caused rubble to use style-based wall graphics instead of the preference file setting

## Solution
Modified the condition in `cave.c` line 1527 to exclude rubble from depth-based wall processing:

**Before:**
```c
if (!graphics_are_ascii() && (feat >= FEAT_WALL_HEAD && feat <= FEAT_WALL_TAIL))
```

**After:**
```c
if (!graphics_are_ascii() && (feat >= FEAT_WALL_HEAD && feat <= FEAT_WALL_TAIL) && feat != FEAT_RUBBLE)
```

## File Modified
- `src/cave.c` - Line 1527

## Expected Result
- Rubble will now display using tile [0,21] as specified in graf-new.prf
- Other wall features (granite, quartz, etc.) continue using the depth-based wall system
- No impact on ASCII mode or other game features

## Compilation Required
The project needs to be recompiled for the fix to take effect:
- Windows: Use the Visual Studio solution in `msvc2022/sil-q.sln` or appropriate Makefile
- Linux/Unix: Use `make -f Makefile.std install` in the src directory

## Verification
After compilation, load a game in graphics mode and verify that rubble piles display as the correct graphics tile rather than appearing as wall tiles.
