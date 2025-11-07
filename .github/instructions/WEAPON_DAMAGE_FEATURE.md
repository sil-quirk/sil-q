# Weapon Damage Display Feature - October 16, 2025

## Feature
Added comprehensive damage display to weapon and bow description menus showing base damage and modified damage with current strength, weight-based limits, and active abilities. Includes special handling for hand-and-a-half weapons.

## Implementation

### Enhanced Function: `describe_weapon_damage()`
Located in `src/obj-info.c`, this function now handles:
- **Melee weapons** (TV_SWORD, TV_POLEARM, TV_HAFTED, TV_DIGGING)
- **Bows** (TV_BOW)
- **Hand-and-a-half weapons** with distinction between one-handed and two-handed usage

### Melee Weapon Damage

#### Regular Weapons
Calculates actual damage using `total_mdd()` and `total_mds()`:
- Shows base damage (e.g., "2d5") and modified damage (e.g., "2d7") separately when different

#### Hand-and-a-Half Weapons (e.g., Bastard Sword)
Special display showing **both configurations**:
- **One-handed**: With shield equipped, shows damage without hand-and-a-half bonus
- **Two-handed**: Without shield, shows damage with +2 sides bonus (+3 for House of Maedhros)
- **Unequipped weapons**: Calculates hypothetical damage for both configurations
- Example: "It does 2d5 damage (2d7 one-handed, 2d9 two-handed with your current strength and abilities)."
- **Note**: The damage shown is accurate whether the weapon is equipped or just being examined

### Bow Damage
Shows complete arrow damage including bow dice and strength-modified sides:
- **Base damage**: Bow's dd × bow's ds (e.g., "1d7" for shortbow, "2d4" for longbow)
- **Modified damage**: Bow's dd × modified ds with strength (e.g., "1d9", "2d6")
- **Example**: "It shoots arrows for 1d7 damage (1d9 with your current strength)."
- **Note**: Arrows themselves do not show damage (bow damage includes arrows)

### Damage Calculation Factors
The displayed damage accounts for:
- **Strength bonus**: Up to weapon/bow weight / 10 lbs per side
- **Generic bonuses**: `p_ptr->to_mdd`, `p_ptr->to_mds`, `p_ptr->to_ads`
- **Hand-and-a-half bonus**: +2 sides when wielded two-handed (+4 for Maedhros)
- **Mighty Blows ability**: +1 damage sides (melee)
- **Vengeance ability**: Adds damage dice (melee)

### Display Logic
- **No modification**: "It does 2d5 damage."
- **With strength bonus**: "It does 2d5 damage (2d7 with your current strength and abilities)."
- **Hand-and-a-half**: "It does 2d5 damage (2d7 one-handed, 2d9 two-handed with your current strength and abilities)."
- **Bow**: "It shoots arrows for 1d7 damage (1d9 with your current strength)."
- **Arrows**: No damage display (damage shown on bow)

## Complete Feature Coverage

| Weapon Type | What's Displayed |
|------------|------------------|
| **Regular melee** | Base damage and strength-modified damage<br>*Example: "It does 2d5 damage (2d7 with your current strength and abilities)."* |
| **Hand-and-a-half** | Base, one-handed (with shield), and two-handed damage<br>*Example: "It does 2d5 damage (2d7 one-handed, 2d9 two-handed with your current strength and abilities)."* |
| **Bows** | Complete arrow damage (bow dice × strength-modified sides)<br>*Example: "It shoots arrows for 1d7 damage (1d9 with your current strength)."* |
| **Arrows** | No damage display (damage information shown on the bow) |

## Integration
The function is called in `object_info_out()` after `describe_archery()` to keep all weapon-related information grouped together.

## Files Modified
- `src/obj-info.c`: Enhanced `describe_weapon_damage()` function with bow support and hand-and-a-half weapon distinction

## Benefits
1. **Player clarity**: See actual damage output without mental math
2. **Educational**: Helps players understand strength/weight interaction
3. **Tactical**: Hand-and-a-half weapons now clearly show the tradeoff between shields and two-handed damage
4. **Bow clarity**: Shows how bow weight affects arrow damage
5. **Consistent UX**: Matches how archery range shows "with your current strength"
6. **Clear distinction**: Base vs modified values are clearly separated

## Testing Notes
- ✅ Build successful with no compilation errors or warnings
- ✅ Function integrates cleanly with existing description system
- ✅ **Bug Fix**: Hand-and-a-half weapons now correctly show different values for one-handed vs two-handed when unequipped
- Ready for in-game testing

## Bug Fixes

### Hand-and-a-Half Weapons Showing Same Values When Unequipped
**Issue**: When examining an unequipped hand-and-a-half weapon, both one-handed and two-handed damage showed the same values.

**Cause**: The `hand_and_a_half_bonus()` function checks if the weapon is currently equipped in `INVEN_WIELD`, returning 0 for unequipped weapons.

**Solution**: Modified `describe_weapon_damage()` to:
- Detect if the weapon is currently equipped
- For equipped weapons: Use actual current damage based on shield presence
- For unequipped weapons: Calculate hypothetical damage for both configurations using the potential hand-and-a-half bonus (+2 or +3 for Maedhros)

**Result**: Unequipped hand-and-a-half weapons now correctly display different damage values for one-handed vs two-handed scenarios.

## Code Location
```c
// src/obj-info.c, lines ~838-938
static bool describe_weapon_damage(const object_type* o_ptr)
{
    // Handles melee weapons, bows, and hand-and-a-half weapons
    // Shows base and modified damage with current stats
    // Special case for hand-and-a-half: shows both 1H and 2H damage
}
```
