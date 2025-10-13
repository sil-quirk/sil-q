# Curse Stacking Bug Fix

## Summary

Fixed a critical bug where curse stacking was not working correctly for flag-based curse effects. The system was only counting the NUMBER of different curses with a flag, not the TOTAL STACKS across all curses with that flag.

## The Bug

**Location:** `src/birth.c` - functions `curse_flag_count_rhf()` and `curse_flag_count_cur()`

**Issue:** When checking for curse effects, the code counted how many different curses had a particular flag, but ignored the stack count for each curse.

**Example:**
```
Before fix:
- Player has 3 stacks of "Echoes of Anguish" (CUR_MON_HP flag)
- curse_flag_count(CUR_MON_HP) returns 1
- Result: Monster HP = base * (100 + 25*1) / 100 = +25% HP only

After fix:
- Player has 3 stacks of "Echoes of Anguish" (CUR_MON_HP flag)
- curse_flag_count(CUR_MON_HP) returns 3
- Result: Monster HP = base * (100 + 25*3) / 100 = +75% HP correctly
```

## Affected Curse Effects

The bug affected ALL curse flags (from `curses.txt` with `U:` lines):

### Monster Buffs
- `CUR_MON_HP` - Ordinary monster HP (+25% per stack)
- `CUR_U_MON_HP` - Unique monster HP (+25% per stack)
- `CUR_MON_STL` - Monster stealth (+2 per stack)
- `CUR_MON_PER` - Monster perception (+2 per stack)
- `CUR_MON_WIL` - Monster will (+2 per stack)
- `CUR_MON_ARM_DICE` - Monster armor dice (+1 per stack)
- `CUR_MON_ARM_SIDE` - Monster armor sides (+1 per stack)

### Spawn/Generation
- `CUR_MON_NUM` - Monster spawn rate (more per stack)
- `CUR_TRAPS` - Trap frequency (more per stack)
- `CUR_FINDCURSE` - Item curse chance (1/20, 1/10, 1/5 per stack)
- `CUR_SMITHCURSE` - Smithed item curse chance

### Player Penalties
- `CUR_LIGHTR` - Light radius reduction (-1 per stack)
- `CUR_LIGHTP` - Light power reduction (darkness +1 level per stack)
- `CUR_WEAK` - Weight limit reduction (20% per stack)
- `CUR_HUNGER` - Digestion rate (2x, 4x, 8x per stack)
- `CUR_DEATH` - Death limit reduction (-3 per stack)
- `CUR_HALLU` - Hallucination on potions (20% per stack)

### Special Flags (Not Affected by Stacking)
- `CUR_NOCHOICE` - Forces random curse selection (binary)
- `CUR_NOSTART` - Start without equipment (binary)

## What Worked Correctly

Stat-based curses (`S:` lines in `curses.txt`) were not affected because they use a different code path (`curses_stat_adj()`) that already multiplied by stack count:

- Strength/Dexterity/Constitution/Grace penalties

## The Fix

Changed the counting logic to sum up all stacks instead of just counting unique curses:

```c
// OLD (buggy) code:
int curse_flag_count_rhf(u32b rhf_flag)
{
    int count = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (curse_count(i) > 0)
        {
            if (cu_info[i].flags & rhf_flag) count++;  // BUG: ignores stacks
        }
    }
    return count;
}

// NEW (fixed) code:
int curse_flag_count_rhf(u32b rhf_flag)
{
    int count = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        int stacks = curse_count(i);
        if (stacks > 0)
        {
            if (cu_info[i].flags & rhf_flag) count += stacks;  // FIX: sum all stacks
        }
    }
    return count;
}
```

Same fix applied to `curse_flag_count_cur()`.

## Impact on Gameplay

This fix significantly increases the difficulty of metarun curse effects when multiple stacks are accumulated:

**Before:** 
- Adding more stacks of the same curse had NO effect on flag-based abilities
- Only the first stack mattered

**After:**
- Each stack properly multiplies the effect
- Curses scale as intended

**Example Scenarios:**
1. **3 stacks of "Echoes of Anguish"** (Monster HP curse)
   - Before: +25% monster HP
   - After: +75% monster HP ✓

2. **2 stacks of "Veil of Shadows"** (Monster Stealth curse)
   - Before: +2 monster stealth
   - After: +4 monster stealth ✓

3. **3 stacks of "Voracious Curse"** (Hunger curse)
   - Before: 2x digestion rate
   - After: 8x digestion rate ✓

## Testing Recommendations

1. Start a metarun with multiple stacks of the same curse
2. Verify monster stats increase correctly (use look command)
3. Test hunger rate with multiple Voracious Curse stacks
4. Confirm light radius reduces properly with multiple Twilight's Shroud stacks
5. Check that death limit decreases correctly with Mandos' Begrudging Mercy stacks

## Files Modified

- `src/birth.c` - Fixed `curse_flag_count_rhf()` and `curse_flag_count_cur()`

## Date

2025-01-11
