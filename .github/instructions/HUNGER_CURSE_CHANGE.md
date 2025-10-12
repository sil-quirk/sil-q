# Hunger Curse Scaling Change

## Summary

Changed the **CUR_HUNGER** (Voracious Curse) implementation from binary doubling (2x, 4x, 8x) to exponential base-3 scaling (3x, 9x, 27x) to match the game's standard hunger rate modifier system.

## How `p_ptr->hunger` Works

**`p_ptr->hunger`** is a **rate modifier** for food digestion, not the food level itself:

- **Type**: `s16b` (signed 16-bit integer)
- **Range**: Can be negative (slower) or positive (faster)
- **Default**: 0 (normal digestion rate)
- **Scaling**: Exponential with base 3
  - `hunger = -3`: Digestion rate is `1/27` normal
  - `hunger = -2`: Digestion rate is `1/9` normal
  - `hunger = -1`: Digestion rate is `1/3` normal (Indomitable ability)
  - `hunger = 0`: Normal digestion rate (1x)
  - `hunger = +1`: Digestion rate is `3x` normal
  - `hunger = +2`: Digestion rate is `9x` normal
  - `hunger = +3`: Digestion rate is `27x` normal

## Food Level Constants

The actual food level (`p_ptr->food`) uses these thresholds:

```c
#define PY_FOOD_MAX     8000    /* Gorged */
#define PY_FOOD_FULL    5000    /* Normal/Well Fed */
#define PY_FOOD_ALERT   2000    /* Hungry */
#define PY_FOOD_WEAK    1000    /* Weak */
#define PY_FOOD_STARVE     1    /* Starving */
```

## Implementation Details

### Before (Binary Doubling)

```c
/* In dungeon.c - digestion calculation */
int h = curse_flag_count(CUR_HUNGER);
if (h) i <<= h;    /* i *= 2^h → 2x, 4x, 8x */
```

This used **bit-shift** for doubling:
- 1 stack: 2x digestion
- 2 stacks: 4x digestion
- 3 stacks: 8x digestion

### After (Base-3 Exponential)

```c
/* In xtra1.c - calc_bonuses() */
int h = curse_flag_count(CUR_HUNGER);
if (h) p_ptr->hunger += h;

/* In dungeon.c - digestion calculation */
/* Removed the direct multiplication, now handled via p_ptr->hunger */
```

This uses the **standard hunger modifier system**:
- 1 stack: `hunger = +1` → 3x digestion
- 2 stacks: `hunger = +2` → 9x digestion
- 3 stacks: `hunger = +3` → 27x digestion

The actual calculation happens in the food digestion code:

```c
/* Basic digestion rate */
i = 1;

// Slow hunger (negative values)
if (p_ptr->hunger < 0)
{
    if (!one_in_(int_exp(3, -(p_ptr->hunger))))
        i = 0;
}
// Fast hunger (positive values)
else if (p_ptr->hunger > 0)
{
    i *= int_exp(3, p_ptr->hunger);  // 3^hunger
}
```

## Comparison Table

| Stacks | Old System (2^n) | New System (3^n) | Difference |
|--------|------------------|------------------|------------|
| 0      | 1x               | 1x               | Same       |
| 1      | 2x               | 3x               | +50%       |
| 2      | 4x               | 9x               | +125%      |
| 3      | 8x               | 27x              | +237%      |

## Rationale

1. **Consistency**: Uses the same exponential system as the Indomitable ability and other hunger modifiers
2. **Game Balance**: Makes the curse more punishing (3x vs 2x for first stack)
3. **Scaling**: Better punishment for stacking the same curse multiple times (27x vs 8x for 3 stacks)
4. **Code Simplicity**: Leverages existing hunger rate infrastructure instead of special-case multiplication

## Interaction with Other Systems

### Indomitable Ability
- Reduces `hunger` by 1 (1/3 normal digestion)
- With 1 CUR_HUNGER stack: `hunger = 0` → normal rate (cancels out)
- With 2 CUR_HUNGER stacks: `hunger = +1` → 3x rate
- With 3 CUR_HUNGER stacks: `hunger = +2` → 9x rate

### Gorged State
- When `food >= PY_FOOD_MAX`, digestion multiplied by 50
- This multiplier is **applied after** the hunger rate calculation
- Example with 2 stacks: `9x (from curse) × 50 (from gorged) = 450x digestion`

### Starvation
- No effect on digestion rate when `food < PY_FOOD_STARVE`
- Player takes 1 HP damage per turn instead

## Files Modified

- `src/xtra1.c` - Added curse effect to `calc_bonuses()` (line ~2810)
- `src/dungeon.c` - Removed direct multiplication, now uses `p_ptr->hunger` (line ~2290)

## Testing Recommendations

1. Start with 1 stack of CUR_HUNGER, verify 3x digestion rate
2. Test with 2 stacks, verify 9x digestion rate  
3. Test with 3 stacks, verify 27x digestion rate
4. Verify Indomitable ability still provides 1/3 digestion
5. Test interaction: Indomitable + 1 CUR_HUNGER stack = normal rate

## Date

2025-01-11
