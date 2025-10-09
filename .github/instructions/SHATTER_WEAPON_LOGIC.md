# Shatter Weapon Logic: Sil-Q vs Sil-More

## Overview

The "shatter weapon" mechanic is part of the Silmaril extraction system. When attempting to prise silmarils from the Iron Crown, there's a chance your weapon will shatter, preventing you from getting the silmaril but angering Morgoth anyway.

## How It Works

### The Basic Flow

When you try to prise a silmaril:
1. You make an attack roll against the crown (uses melee skill)
2. If you deal damage, you might succeed in freeing the silmaril
3. **BUT** for the 2nd and 3rd silmarils, there's a shatter check:
   - **1st Silmaril:** No shatter risk - always succeeds if you deal damage
   - **2nd Silmaril:** 50% chance weapon shatters (only once per character)
   - **3rd Silmaril:** 100% chance weapon shatters (only once per character)

### The Independent Shatter Flags (Sil-More)

**IMPORTANT:** Each silmaril has its own independent shatter flag!

- `crown_shatter_sil2`: Set to `true` if weapon shatters on 2nd silmaril
- `crown_shatter_sil3`: Set to `true` if weapon shatters on 3rd silmaril

**This means:**
- Your weapon can shatter TWICE in total (once on 2nd, once on 3rd)
- Each silmaril remembers if it has caused a shatter
- After shattering on 2nd, you're safe from 2nd shatter but NOT from 3rd!
- After shattering on 3rd, you're safe from 3rd shatter

### The Shatter Check Logic (Updated Sil-More)

```c
case ART_MORGOTH_2:  // 2nd silmaril
{
    if (!p_ptr->crown_shatter_sil2 && one_in_(2))  // 50% chance if NEVER shattered on 2nd
    {
        shatter_weapon(2);
        p_ptr->crown_shatter_sil2 = true;  // Flag this specific silmaril
        freed = false;  // You DON'T get the silmaril
    }
    else  // Either already shattered on 2nd, or passed the 50% check
    {
        // Success! You get the silmaril
        // Morgoth gets angry if he sees
    }
}

case ART_MORGOTH_1:  // 3rd silmaril
{
    if (!p_ptr->crown_shatter_sil3)  // 100% chance if NEVER shattered on 3rd
    {
        shatter_weapon(3);
        p_ptr->crown_shatter_sil3 = true;  // Flag this specific silmaril
        freed = false;  // You DON'T get the silmaril
    }
    else  // Already shattered on 3rd - you're protected
    {
        p_ptr->cursed = true;  // But you get cursed instead!
        // You DO get the silmaril
    }
}
```

## What Happens When Weapon Shatters

### Sil-More (Current - FIXED)

```c
void shatter_weapon(int silnum)
{
    p_ptr->crown_shatter = true;  // Set the flag
    
    // Destroy the weapon
    inven_item_increase(INVEN_WIELD, -1);
    inven_item_optimize(INVEN_WIELD);
    
    // Determine anger level based on which silmaril
    anger_level = (silnum == 2) ? 3 : 4;  // ← CORRECT!
    
    // Check if Morgoth sees (within 5 squares, has LOS)
    if (morgoth_nearby_and_can_see)
    {
        msg_print("A shard strikes Morgoth upon his cheek.");
        set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
        anger_morgoth(anger_level);  // 2nd sil → state 3, 3rd sil → state 4
    }
}
```

**Messages:**
- 2nd Silmaril: "You strive to free a second Silmaril, but it is not fated to be."
- 3rd Silmaril: "You strive to free a third Silmaril, but it is not fated to be."
- Then: "As you strike the crown, your [weapon] shatters into innumerable pieces."
- If Morgoth sees: "A shard strikes Morgoth upon his cheek."

### Original Sil-Q (BUGGY)

```c
void shatter_weapon(int silnum)
{
    // Same destruction logic...
    
    // BUG: Always calls anger_morgoth(2) regardless of which silmaril!
    if (morgoth_nearby_and_can_see)
    {
        msg_print("A shard strikes Morgoth upon his cheek.");
        set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
        anger_morgoth(2);  // ← WRONG! Should be 3 for 2nd sil, 4 for 3rd sil
    }
}
```

## Comparison Table

| Scenario | Original Sil-Q | Sil-More (Fixed) |
|----------|---------------|------------------|
| **1st silmaril** | No shatter check | No shatter check ✅ |
| **2nd silmaril (never shattered)** | 50% shatter → State 2 ❌ | 50% shatter → State 3 ✅ |
| **2nd silmaril (already shattered)** | Success, anger if seen → State 2 ❌ | Success, anger if seen → State 3 ✅ |
| **3rd silmaril (never shattered)** | 100% shatter → State 2 ❌ | 100% shatter → State 4 ✅ |
| **3rd silmaril (already shattered)** | Success, cursed, no anger | Success, cursed, no anger ✅ |

## Strategic Implications

### The Independent Shatter System

Each silmaril now has its own shatter "memory":

- **2nd silmaril:** 50% shatter chance (once only)
- **3rd silmaril:** 100% shatter chance (once only)
- **They are INDEPENDENT** - you can lose weapons on both!

### Possible Scenarios

**Scenario 1: Lucky Run (Best Case)**
1. Get 1st silmaril (no risk) ✅
2. Try 2nd silmaril - pass 50% check ✅
3. Try 3rd silmaril - weapon SHATTERS ❌
4. Get new weapon, try 3rd again - SUCCESS (curse applied) ✅

**Scenario 2: Unlucky on 2nd (Common)**
1. Get 1st silmaril ✅
2. Try 2nd silmaril - SHATTER (50% failed) ❌
3. Get new weapon, try 2nd again - SUCCESS ✅
4. Try 3rd silmaril - SHATTER (100%) ❌
5. Get new weapon, try 3rd again - SUCCESS (curse applied) ✅

**Scenario 3: Perfect Preparation (Need 3 Weapons)**
1. Get 1st silmaril with weapon #1 ✅
2. Try 2nd with weapon #1 - SHATTER ❌
3. Try 2nd with weapon #2 - SUCCESS ✅
4. Try 3rd with weapon #2 - SHATTER ❌
5. Try 3rd with weapon #3 - SUCCESS ✅

**Scenario 4: Maximum Unlucky (Worst Case)**
- Shatter on 2nd: Need new weapon
- Get 2nd successfully
- Shatter on 3rd: Need ANOTHER new weapon
- Get 3rd successfully (cursed)
- **Total: 2 weapons lost + curse**

### Optimal Strategy

**Conservative approach (bring 3 good weapons):**
1. Use weapon #1 for 1st and 2nd silmarils
2. If shatter on 2nd, use weapon #2 to complete 2nd
3. Use weapon #2 (or #3) for 3rd silmaril - WILL shatter
4. Use weapon #3 (or #2) to complete 3rd - cursed but armed

**Aggressive approach (bring 2 good weapons, accept risk):**
1. Use weapon #1 for 1st and 2nd
2. If shatter, switch to weapon #2
3. If lucky on 2nd, weapon #2 WILL shatter on 3rd
4. If unlucky on 2nd, use weapon #2 for 2nd (success) and 3rd (shatter)
5. Either way, you'll need to go back for another weapon OR accept curse

**The key difference:** You now need to plan for TWO potential shatters instead of one!

## Vision/Distance Requirements for Anger

**Important:** The shatter anger check has the SAME requirements as normal silmaril anger:

```c
if ((m_ptr->cdis <= 5)  // Morgoth within 5 squares
    && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))  // Line of sight
{
    // Flying shard hits Morgoth, he gets VERY angry
    anger_morgoth(anger_level);
}
```

**No alertness check for shatter!** Unlike successful silmaril extraction (which requires `ALERTNESS_ALERT`), the flying weapon shard can hit Morgoth even if he's unaware.

This makes sense thematically - a piece of your weapon physically strikes him on the cheek!

## Sil-More Fixes Summary

1. **Correct anger levels:** 2nd sil shatter → state 3, 3rd sil shatter → state 4
2. **Added logging:** Track which silmaril, anger level, and Morgoth's position
3. **Consistent with other changes:** Works with cumulative state system

## Edge Cases

### What if you have no weapon?

The code accesses `inventory[INVEN_WIELD]` directly - if empty, you'd have problems. In practice, you NEED a weapon to prise silmarils (it's an attack roll), so this shouldn't happen.

### What if Morgoth is dead?

The loop checks all monsters for `R_IDX_MORGOTH` - if he's dead, no monster matches, no anger.

### What if you're on the run?

Shatter can still happen, and if Morgoth is nearby and sees, he still gets angry. The pursuit anger check (in `do_cmd_go_up`) happens separately when you leave the level.

## Thematic Interpretation

The shatter mechanic represents:
- **Difficulty:** The crown's magic resists your attempts
- **Risk/reward:** You might lose your weapon trying for more power
- **Learning:** Once you've seen it happen, you know how to avoid it (crown_shatter flag)
- **Fury:** Morgoth is even MORE angry when your failed attempt physically strikes him

The BUG in original Sil-Q meant the drama of weapon-shattering didn't match the mechanical consequence - he should be MUCH angrier when you fail on the 3rd silmaril than the 2nd!
