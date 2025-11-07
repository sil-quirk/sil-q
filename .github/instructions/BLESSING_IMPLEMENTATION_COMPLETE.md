# Blessing System Implementation - Complete

## Summary of Changes

### 1. Fixed Blessing Filter Bug
**File:** `src/init1.c` line 4775
- **Bug:** All curses had `blessing_name = curse_name` by default
- **Fix:** Changed to `blessing_name = 0` (NULL unless B: directive exists)
- **Result:** Only curses with explicit B: fields appear in blessing menu

### 2. Removed Mandos' Begrudging Mercy Blessing
**File:** `lib/edit/curses.txt` curse ID 26
- Removed B:, E:, H: fields
- Reason: Game no longer has maximum deaths mechanic
- Curse remains (for backward save compatibility) but cannot be blessed

### 3. Added Blessing Definitions
**File:** `lib/edit/curses.txt`

Added B:, E:, H: fields for curses 13-20, 24-25, 29:

| ID | Curse | Blessing | Effect |
|----|-------|----------|--------|
| 13 | Burden of the Exile | Blessing of the Unburdened | Weight ±20% |
| 14 | Echoes of Anguish | Blessing of Weakening Shadow | Monster HP ±10% |
| 15 | Doom of the Mighty | Blessing of Humbled Legends | Unique HP ±10% |
| 16 | Veil of Shadows | Blessing of Revealing Light | Monster Stl ±2 |
| 17 | Lidless Gaze | Blessing of Clouded Sight | Monster Per ±2 |
| 18 | Iron Will of Morgoth | Blessing of Shattered Resolve | Monster Wil ±2 |
| 19 | Scales of Anfauglith | Blessing of Brittle Armor | Monster armor dice ±1 |
| 20 | Halls of Adamant | Blessing of Sundered Steel | Monster armor sides ±1 |
| 24 | Twilight's Shroud | Blessing of Radiant Dawn | Light radius ±1 |
| 25 | Waning of the Lamps | Blessing of Undying Flame | Light power ±1 |
| 29 | Voracious Curse | Blessing of Sustenance | Digestion rate ±1 |

**Note:** User updated HP percentages from 20% to 10% for balance

### 4. Implemented Blessing Effects in Code

#### Weight Limit (CUR_WEAK)
**File:** `src/xtra1.c` ~line 2165
```c
int weak_stacks = curse_flag_count_cur(CUR_WEAK);
if (weak_stacks > 0) {
    for (i = 0; i < weak_stacks; i++) limit *= 0.8;   // Curse: -20% per stack
} else if (weak_stacks < 0) {
    for (i = 0; i < -weak_stacks; i++) limit *= 1.2;  // Blessing: +20% per stack
}
```

#### Monster HP (CUR_MON_HP, CUR_U_MON_HP)
**File:** `src/monster2.c` ~line 2575
```c
int stacks = curse_flag_count_cur(CUR_MON_HP);  // or CUR_U_MON_HP for uniques
if (stacks != 0)
    n_ptr->maxhp = (n_ptr->maxhp * (100 + 10 * stacks)) / 100;  // ±10% per stack
```

#### Monster Skills (CUR_MON_STL, CUR_MON_PER, CUR_MON_WIL)
**File:** `src/monster2.c` ~line 1187
```c
skill = r_ptr->stl;  // or per/wil
skill += 2 * curse_flag_count_cur(CUR_MON_STL);  // ±2 per stack
```

#### Monster Armor Dice/Sides (CUR_MON_ARM_DICE, CUR_MON_ARM_SIDE)
**Files:** `src/cmd1.c`, `src/cmd2.c` (2 locations), `src/spells2.c`

Added to all damage calculation locations:
```c
int armor_dice = r_ptr->pd + curse_flag_count_cur(CUR_MON_ARM_DICE);
int armor_sides = r_ptr->ps + curse_flag_count_cur(CUR_MON_ARM_SIDE);
if (armor_dice < 0) armor_dice = 0;
if (armor_sides < 1) armor_sides = 1;
prt = damroll(armor_dice, armor_sides);
```

#### Light Radius (CUR_LIGHTR)
**File:** `src/xtra1.c` ~line 1998
```c
int r = curse_flag_count_cur(CUR_LIGHTR);
if (r != 0)
    p_ptr->cur_light = MAX(0, p_ptr->cur_light - r);  // ±1 per stack
```

#### Light Power (CUR_LIGHTP)
**File:** `src/cave.c` ~line 4232
- Already works correctly: `cave_light[y][x] -= dark_stacks;`
- Negative stacks (blessings) add to light power
- Updated comment to reflect blessing support

#### Hunger (CUR_HUNGER)
**File:** `src/xtra1.c` ~line 2810
```c
int h = curse_flag_count_cur(CUR_HUNGER);
if (h != 0) p_ptr->hunger += h;  // +h for curse, -h for blessing
```

## How Blessing Effects Work

All curse effects now support **signed stack counts**:
- **Positive stacks** = curse effect (penalties)
- **Negative stacks** = blessing effect (bonuses)

The `curse_flag_count_cur()` function returns:
- Positive number for curses (number of curse stacks)
- Negative number for blessings (number of blessing stacks)
- Zero if neither curse nor blessing is active

By changing `if (stacks > 0)` to `if (stacks != 0)` and using the sign, all effects automatically work bidirectionally.

## Curses Without Blessings (Intentional)

These remain curse-only:
- **12 - Fate's Tyranny**: Meta-mechanic (no choice in curse selection)
- **21 - The Naked Path**: One-time start condition
- **22 - Blacksmith's Folly**: Probabilistic crafting penalty
- **23 - Morgoth's Mark**: Probabilistic item spawning
- **26 - Mandos' Begrudging Mercy**: Death limit (removed from game)
- **27 - Maze of Malice**: Environmental trap density
- **28 - Host of Angband**: Environmental monster density
- **30 - Draught of Delirium**: Probabilistic potion hallucination

## Testing Checklist

- [ ] Weight limit increases/decreases with blessings/curses
- [ ] Monster HP affected by blessings (both normal and unique)
- [ ] Monster stealth/perception/will modified by blessings
- [ ] Monster armor reduced by blessings (dice and sides)
- [ ] Light radius increases with blessings
- [ ] Light becomes brighter with blessings
- [ ] Hunger decreases with blessings
- [ ] Blessing menu only shows curses with B: fields defined
- [ ] Curse 26 (Mandos') does NOT appear in blessing menu
- [ ] Cannot select blessing if same curse is active

## Build Status

✅ Build successful with all changes
- No compilation errors
- Only standard warnings (unused parameters, sign comparisons)

## Date
October 14, 2025
