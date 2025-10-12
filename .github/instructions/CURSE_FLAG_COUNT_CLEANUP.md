# Curse Flag Count Function Cleanup

## Issue
The legacy `curse_flag_count()` function was causing incorrect calculations by summing both RHF and CUR flags, which share bit positions. This led to false positives and incorrect skill affinity/penalty calculations.

## Root Cause
The codebase has two separate flag types:
- **RHF flags** (Race/House Flags): `RHF_MEL_AFFINITY`, `RHF_SNG_PENALTY`, etc. - stored in `cu_info[].flags`
- **CUR flags** (Curse Flags): `CUR_LIGHTR`, `CUR_HUNGER`, `CUR_MON_HP`, etc. - stored in `cu_info[].flags_u`

These flag sets share bit positions (e.g., both use `0x00000001L`, `0x00000002L`, etc.), so a legacy function that adds counts from both sets will produce incorrect results when a CUR flag happens to have the same bit value as an RHF flag.

## Solution
Removed the legacy `curse_flag_count()` function entirely and updated all callers to use the appropriate specific function:
- **`curse_flag_count_rhf()`** - for RHF flags (skill affinities/penalties)
- **`curse_flag_count_cur()`** - for CUR flags (game mechanic curses)

## Files Modified

### 1. `src/birth.c`
- **Removed**: Legacy `curse_flag_count()` function definition (lines 934-937)
- **Reason**: Function is unsafe and causes false positives

### 2. `src/files.c`
- **Changed**: Character sheet `HANDLE_SKILL_EX` macro (lines 1415-1416)
- **From**: `curse_flag_count(AFF_FLAG)` / `curse_flag_count(PEN_FLAG)`
- **To**: `curse_flag_count_rhf(AFF_FLAG)` / `curse_flag_count_rhf(PEN_FLAG)`
- **Reason**: Skill affinities/penalties use RHF flags

### 3. `src/xtra1.c`
- **Changed**: `affinity_level()` function (lines 2042-2043)
  - **From**: `curse_flag_count(affinity_flag)` / `curse_flag_count(penalty_flag)`
  - **To**: `curse_flag_count_rhf(affinity_flag)` / `curse_flag_count_rhf(penalty_flag)`
  - **Reason**: Affinity/penalty flags are RHF flags

- **Changed**: Light radius curse (line 1998)
  - **From**: `curse_flag_count(CUR_LIGHTR)`
  - **To**: `curse_flag_count_cur(CUR_LIGHTR)`
  - **Reason**: `CUR_LIGHTR` is a CUR flag

- **Changed**: Weak curse (line 2163)
  - **From**: `curse_flag_count(CUR_WEAK)`
  - **To**: `curse_flag_count_cur(CUR_WEAK)`
  - **Reason**: `CUR_WEAK` is a CUR flag

- **Changed**: Hunger curse (line 2810)
  - **From**: `curse_flag_count(CUR_HUNGER)`
  - **To**: `curse_flag_count_cur(CUR_HUNGER)`
  - **Reason**: `CUR_HUNGER` is a CUR flag

### 4. `src/monster2.c`
- **Changed**: Monster skills (lines 1187, 1191, 1195)
  - **From**: `curse_flag_count(CUR_MON_STL)` / `CUR_MON_PER` / `CUR_MON_WIL`
  - **To**: `curse_flag_count_cur()` for all
  - **Reason**: All are CUR flags

- **Changed**: Monster HP curses (lines 2575, 2587)
  - **From**: `curse_flag_count(CUR_U_MON_HP)` / `CUR_MON_HP`
  - **To**: `curse_flag_count_cur()` for both
  - **Reason**: Both are CUR flags

### 5. `src/use-obj.c`
- **Changed**: Hallucination curse (line 638)
  - **From**: `curse_flag_count(CUR_HALLU)`
  - **To**: `curse_flag_count_cur(CUR_HALLU)`
  - **Reason**: `CUR_HALLU` is a CUR flag

### 6. `src/metarun.c`
- **Changed**: Death limit calculation (line 2202)
  - **From**: `curse_flag_count(CUR_DEATH)`
  - **To**: `curse_flag_count_cur(CUR_DEATH)`
  - **Reason**: `CUR_DEATH` is a CUR flag

### 7. `src/generate.c`
- **Changed**: Cursed items (line 1081)
  - **From**: `curse_flag_count(CUR_FINDCURSE)`
  - **To**: `curse_flag_count_cur(CUR_FINDCURSE)`

- **Changed**: Trap generation (line 2315)
  - **From**: `curse_flag_count(CUR_TRAPS)`
  - **To**: `curse_flag_count_cur(CUR_TRAPS)`

- **Changed**: Monster count (line 5165)
  - **From**: `curse_flag_count(CUR_MON_NUM)`
  - **To**: `curse_flag_count_cur(CUR_MON_NUM)`
  - **Reason**: All are CUR flags

### 8. `src/cmd1.c`
- **Changed**: Monster armor protection (line 5030)
  - **From**: `curse_flag_count(CUR_MON_ARM_DICE)` / `CUR_MON_ARM_SIDE`
  - **To**: `curse_flag_count_cur()` for both
  - **Reason**: Both are CUR flags

### 9. `src/cmd4.c`
- **Changed**: Smithing curse (line 7290)
  - **From**: `curse_flag_count(CUR_SMITHCURSE)`
  - **To**: `curse_flag_count_cur(CUR_SMITHCURSE)`
  - **Reason**: `CUR_SMITHCURSE` is a CUR flag

### 10. `src/cave.c`
- **Changed**: Light power curse (line 4232)
  - **From**: `curse_flag_count(CUR_LIGHTP)`
  - **To**: `curse_flag_count_cur(CUR_LIGHTP)`
  - **Reason**: `CUR_LIGHTP` is a CUR flag

### 11. `src/externs.h`
- **Removed**: `extern int curse_flag_count(u32b flag);` declaration (line 1339)

### 12. `src/metarun.h`
- **Removed**: `int curse_flag_count(u32b flag);` declaration (line 202)

## Testing
- ✅ Full build successful with no errors
- ✅ Character sheet now correctly displays skill affinities/penalties
- ✅ Curses menu matches character sheet display
- ✅ All curse mechanics use the correct flag-counting function

## Benefits
1. **Correctness**: Eliminates false positives from mixed flag counting
2. **Clarity**: Each call explicitly shows whether it's checking RHF or CUR flags
3. **Maintainability**: Removes a confusing legacy function that could mislead developers
4. **Type Safety**: Prevents future bugs by forcing developers to choose the correct function

## Flag Reference
### RHF Flags (use `curse_flag_count_rhf()`)
- Skill affinities: `RHF_MEL_AFFINITY`, `RHF_SNG_AFFINITY`, etc.
- Skill penalties: `RHF_MEL_PENALTY`, `RHF_SNG_PENALTY`, etc.
- Proficiencies: `RHF_BOW_PROFICIENCY`, `RHF_AXE_PROFICIENCY`
- Special flags: `RHF_KINSLAYER`, `RHF_TREACHERY`, `RHF_CURSE`, etc.

### CUR Flags (use `curse_flag_count_cur()`)
- Game mechanics: `CUR_WEAK`, `CUR_HUNGER`, `CUR_HALLU`
- Monster modifiers: `CUR_MON_HP`, `CUR_MON_STL`, `CUR_MON_ARM_DICE`
- Light/Vision: `CUR_LIGHTR`, `CUR_LIGHTP`
- Generation: `CUR_FINDCURSE`, `CUR_TRAPS`, `CUR_MON_NUM`
- Meta: `CUR_NOCHOICE`, `CUR_NOSTART`, `CUR_DEATH`

## Date
October 12, 2025
