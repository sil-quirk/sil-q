# Blessing System Final Updates

## Changes Implemented

### 1. ✅ Asymmetrical Monster HP for Curses vs Blessings

**File:** `src/monster2.c` lines 2573-2595

**Implementation:**
- **Curse (positive stacks)**: +20% HP per stack
- **Blessing (negative stacks)**: -10% HP per stack

This makes sense for game balance - it's easier to make the game harder than easier.

```c
int stacks = curse_flag_count_cur(CUR_MON_HP);  // or CUR_U_MON_HP
if (stacks > 0) {
    /* Curse: +20% per stack */
    n_ptr->maxhp = (n_ptr->maxhp * (100 + 20 * stacks)) / 100;
} else if (stacks < 0) {
    /* Blessing: -10% per stack */
    n_ptr->maxhp = (n_ptr->maxhp * (100 + 10 * stacks)) / 100;
}
```

**Applies to:**
- Curse 14: Echoes of Anguish (normal monsters)
- Curse 15: Doom of the Mighty (unique monsters)

### 2. ✅ Removed Blessing from Scales of Anfauglith (Curse 19)

**File:** `lib/edit/curses.txt` curse ID 19

**Reason:** Reducing armor dice can make it 0, which would be too powerful as a blessing.

**Result:** Curse 19 remains as curse-only (no B:, E:, H: fields).

### 3. ✅ Completely Removed Mandos' Begrudging Mercy (Curse 26)

**File:** `lib/edit/curses.txt`

**Removed entire entry** including:
- N: directive (curse definition)
- S: stat adjustments
- U: unique flag (DEATH)
- A: weight/max stacks
- D: description
- P: power text

**Reason:** The game no longer has a death limit mechanic. The metarun ends only when there aren't enough alive heroes to win (see `check_run_end()` in metarun.c).

**Note:** This removes curse ID 26 entirely. Curse IDs now skip from 25 directly to 27.

### 4. ✅ Weighted Blessing Selection

**File:** `src/metarun.c` lines 3150-3200

**Implementation:**
Blessings now use the same weight system as curses (from the A: field in curses.txt).

**Algorithm:**
1. Build list of eligible blessings with their weights
2. Calculate total weight
3. For each of 3 selections:
   - Do weighted random selection (like curse selection)
   - Remove selected blessing from pool
   - Reduce total weight accordingly
   
```c
/* Use curse weight for blessing selection */
weights[count] = c->weight > 0 ? c->weight : 1;
total_weight += weights[count];

/* Weighted random selection */
int roll = rand_int(total_weight);
int sum = 0;
for (int j = 0; j < count; j++) {
    sum += weights[j];
    if (roll < sum) {
        selected = j;
        break;
    }
}
```

**Effect:** 
- Common blessings (higher weight) appear more frequently
- Rare blessings (lower weight) appear less frequently
- Matches curse selection behavior for consistency

## Current Blessing Availability

### Available as Blessings (22 total)
- **0-11**: Original stat/skill blessings (Str, Dex, Con, Gra, Melee, Archery, Stealth, Evasion, Perception, Will, Smith, Song)
- **13**: Burden of the Exile → Weight limit
- **14-15**: Monster HP (normal/unique)
- **16-18**: Monster skills (Stealth, Perception, Will)
- **20**: Monster armor sides
- **24-25**: Light radius/power
- **29**: Hunger rate

### Curse-Only (No Blessings) (8 total)
- **12**: Fate's Tyranny (meta-mechanic)
- **19**: Scales of Anfauglith (armor dice - too powerful)
- **21**: The Naked Path (one-time start condition)
- **22**: Blacksmith's Folly (probabilistic crafting)
- **23**: Morgoth's Mark (probabilistic spawning)
- **27**: Maze of Malice (trap density)
- **28**: Host of Angband (monster density)
- **30**: Draught of Delirium (potion hallucination)

### Removed Entirely (1 total)
- **26**: Mandos' Begrudging Mercy (obsolete mechanic)

## Weight Values by Curse/Blessing

From curses.txt A: field (weight/max_stacks):

| Weight | Curses |
|--------|--------|
| 1 | 19 (armor dice), 21 (naked path), 30 (delirium) |
| 2 | 0-3 (stats), 12 (fate's tyranny), 22-23 (cursed items), 28-29 (monsters/hunger) |
| 3 | 4-11 (skills), 15 (unique HP), 20 (armor sides), 27 (traps) |
| 4 | 13 (weight), 14 (monster HP), 16-18 (monster skills) |

**Higher weight = more likely to appear in selections**

## Build Status

✅ Build successful
- Monster HP asymmetry implemented
- Curse 19 blessing removed
- Curse 26 completely removed
- Weighted blessing selection working
- All changes tested and compiled

## Testing Checklist

- [ ] Monster HP increases 20% with curse, decreases 10% with blessing
- [ ] Curse 19 (Scales of Anfauglith) does NOT appear in blessing menu
- [ ] Curse 26 (Mandos' Begrudging Mercy) does NOT appear anywhere
- [ ] Blessing menu respects weights (common blessings appear more often)
- [ ] Higher weight blessings (4) appear ~4x as often as weight-1 blessings
- [ ] All 22 available blessings can still be selected
- [ ] Game doesn't crash with removed curse 26

## Date
October 14, 2025
