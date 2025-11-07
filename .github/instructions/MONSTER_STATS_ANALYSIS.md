# Monster Stats and Skills: Modifiability and Persistence Analysis

**Date**: October 19, 2025  
**Repository**: Sil-More (Deaths-redesign branch)  
**Scope**: Monster stats/skills update potential and save/load persistence

---

## Executive Summary

**Key Finding**: Monster stats and skills **CANNOT be permanently modified during gameplay** in the traditional sense. However, many **runtime properties ARE saved and restored**. The distinction is critical:

- **Racial Stats** (damage dice, hp dice, perception, stealth, will) are **read-only** from the race definition (`r_info[]`) and **cannot be modified**
- **Instance Properties** (hp, alertness, stunned, confused, hasted, slowed, etc.) **ARE modifiable and persisted** to save files

---

## Monster Data Structure Overview

### Monster Type Structure (`struct monster_type` in `types.h`)

The instance-level monster data that exists per-monster (as opposed to race-level data):

```c
struct monster_type {
    s16b r_idx;              // Race index (immutable reference)
    byte fy, fx;             // Position on map
    s16b hp;                 // Current Hit Points (MUTABLE & SAVED)
    s16b maxhp;              // Max Hit Points (MUTABLE & SAVED)
    
    s16b alertness;          // Awareness state (MUTABLE & SAVED)
    byte skip_next_turn;     // Turn skip flag (MUTABLE & SAVED)
    byte mspeed;             // Speed override (MUTABLE & SAVED)
    byte energy;             // Energy counter (MUTABLE & SAVED)
    
    byte stunned;            // Stun counter (MUTABLE & SAVED)
    byte confused;           // Confusion counter (MUTABLE & SAVED)
    s16b slowed;             // Slowed counter (MUTABLE & SAVED)
    s16b hasted;             // Hasted counter (MUTABLE & SAVED)
    
    byte stance;             // Combat stance (MUTABLE & SAVED)
    s16b morale;             // Morale value (MUTABLE & SAVED)
    s16b tmp_morale;         // Temp morale modifier (MUTABLE & SAVED)
    
    byte mana;               // Current mana (MUTABLE & SAVED)
    byte song;               // Current song index (MUTABLE & SAVED)
    
    s16b consecutive_attacks; // Attack streak (MUTABLE & SAVED)
    s16b turns_stationary;    // Idle counter (MUTABLE & SAVED)
    // ... and more tracking fields
};
```

### Monster Race Structure (`struct monster_race` in `types.h`)

The **immutable racial definition** - these are **NEVER modified for individual instances**:

```c
struct monster_race {
    u32b name;              // Race name
    byte hdice, hside;      // Hit dice (hp formula)
    s16b evn;               // Evasion bonus (fixed)
    byte pd, ps;            // Protection dice/sides (fixed)
    byte speed;             // Base speed (fixed)
    s16b per, stl, wil;     // Perception, Stealth, Will (FIXED RACIAL VALUES)
    byte freq_ranged;       // Ranged frequency (fixed)
    byte spell_power;       // Spell power (fixed)
    monster_blow blow[4];   // Attack definitions (fixed)
    byte level;             // Monster level (fixed)
    // ... flag fields
};
```

---

## Monster Stats: What They Are and What They Affect

### 1. **Derived Stats** (Calculated at Runtime)

These are **computed from the monster instance + race** and are **READ-ONLY**:

#### `monster_stat()` Function (lines 1219-1262 in `monster2.c`)

Calculates derived stats for monsters:

| Stat | Formula | Source | Use | Modifiable? |
|------|---------|--------|-----|-------------|
| **A_STR** | `(blow[0].dd * 2) + (hdice/10) - 4` | Melee Damage dice + HD | Melee damage rolls, grapples | ❌ NO (race-based) |
| **A_CON** | Inverse calculation from `maxhp` | Current HP pool | Stamina checks, poison resistance | ❌ NO (derived from hp) |
| **A_DEX** | Not calculated | N/A | Not used for monsters | ❌ N/A |
| **A_GRA** | Not calculated | N/A | Not used for monsters | ❌ N/A |

```c
// Example from monster_stat():
case A_STR:
    stat = (r_ptr->blow[0].dd * 2) + (r_ptr->hdice / 10) - 4;
    break;
```

### 2. **Racial Skills** (Immutable)

These skills are **derived from the race definition** and **cannot be changed per-instance**:

#### `monster_skill()` Function (lines 1169-1218 in `monster2.c`)

| Skill | Source | Range | Use | Modifiable? |
|-------|--------|-------|-----|-------------|
| **S_PER** (Perception) | `r_ptr->per` + CUR_MON_PER curses | Typically 10-20 | Monster spotting, surprise attacks | ⚠️ ONLY via curses |
| **S_STL** (Stealth) | `r_ptr->stl` + CUR_MON_STL curses | Typically 5-15 | Sneaking, ambush difficulty | ⚠️ ONLY via curses |
| **S_WIL** (Will) | `r_ptr->wil` + CUR_MON_WIL curses | Typically 8-18 | Willpower checks, song resistance | ⚠️ ONLY via curses |
| **S_MEL, S_ARC, S_EVN, S_SMT, S_SNG** | Not calculated | N/A | Debug only | ❌ NO |

**Special**: Stunning penalizes all skills by -2:
```c
if (m_ptr->stunned)
    skill -= 2;
```

### 3. **Modifiable Runtime Properties** (SAVED & RESTORED)

These **CAN be modified during gameplay** and **ARE persisted** to save files:

| Property | Type | Range | Effect | How Modified |
|----------|------|-------|--------|--------------|
| **hp** | s16b | 1 to maxhp | Direct damage scaling | `m_ptr->hp -= damage` |
| **maxhp** | s16b | Per race | HP cap changes | Monster creation only |
| **alertness** | s16b | Various | Awareness level (ALERT, UNWARY, ASLEEP) | `set_alertness()`, combat |
| **stunned** | byte | 0-255 turns | Stun duration, -2 skill penalty | Status effects |
| **confused** | byte | 0-255 turns | Confusion duration | Status effects |
| **hasted** | s16b | Turns | Speed boost | Spells, effects |
| **slowed** | s16b | Turns | Speed reduction | Spells, effects |
| **mana** | byte | 0-255 | Spell casting pool | Monster spellcasting |
| **song** | byte | Song index | Current song effect | Monster song casting |
| **morale** | s16b | Unlimited | Combat morale/fleeing | Combat engagement |
| **stance** | byte | FLEEING/TIMID/CAUTIOUS/AGGRESSIVE | Combat behavior | Monster AI |
| **energy** | byte | 0-MAX | Action queue | Turn system |
| **mflag** | u32b | Bit flags | Active effects (SUMMONED, CHARGED, etc.) | Flag operations |

---

## Save/Load Persistence Analysis

### What Gets Saved (`wr_monster()` in `save.c:552-600`)

The following fields are **explicitly written to save files**:

```
r_idx, image_r_idx, fy, fx, hp, maxhp, alertness, skip_next_turn,
mspeed, energy, stunned, confused, hasted, slowed, stance, morale,
tmp_morale, noise, encountered, target_y, target_x, wandering_idx,
wandering_dist, mana, song, skip_this_turn, consecutive_attacks,
turns_stationary, mflag, previous_action[ACTION_MAX]
+ 8 spare bytes for future use
```

### What Gets Loaded (`rd_monster()` in `load.c:551-600`)

All fields above are **restored exactly** when a save is loaded.

### Key Implications

✅ **What Persists Across Save/Load**:
- Monster position and instance ID
- Current HP and status effects (stun, slow, haste, confusion)
- Alertness/awareness state
- Morale, stance, energy
- Spellcasting state (mana, song)
- Combat history (consecutive attacks, turns stationary)
- Active flags (summoned status, charge effects)

❌ **What Does NOT Persist** (Recomputed):
- Racial stats (re-derived from r_ptr on demand)
- Derived attributes (A_STR, A_CON calculated on-the-fly)
- Racial skills (pulled from r_ptr->per/stl/wil on-the-fly)
- Non-saved flags (min_range, best_range - marked "Not saved")

---

## How Monster Stats Are Used in Gameplay

### Combat Calculations

**Example: Melee Attack**
- Uses `monster_stat(m_ptr, A_STR) * 2` as skill check base
- **Cannot be modified** during combat—only via damage → maxhp → derived A_CON change

**Example: Perception vs. Stealth**
- Uses `monster_skill(m_ptr, S_PER)` to detect player
- **Can only be penalized** by CUR_MON_PER curses or stunning

### Spell & Song Effects

```c
// Example: Monster song resistance (melee2.c:5622)
difference = MAX(p_ptr->skill_use[S_WIL] - monster_skill(m_ptr, S_WIL), 0);
```
- Monster will is pulled from race, not modifiable per-instance

### Status Effects That Are Modifiable

- **Haste/Slow**: Applied via spells, affects speed
- **Stun/Confuse**: Applied via hits, lowers skills by 2
- **Morale**: Affects AI decisions, changes in combat
- **Alertness**: Critical for combat engagement

---

## Technical Architecture

### Monster Generation Flow

1. `get_mon_num()` → Selects a race index
2. `monster_place()` → Creates a `monster_type` instance
3. Initialize instance fields (hp = maxhp, alertness = default, etc.)
4. **Race definition is linked but never copied** to monster_type

### Runtime Query Pattern

```c
// Monster stats/skills are NEVER stored; always queried from race
monster_race* r_ptr = &r_info[m_ptr->r_idx];
int perception = r_ptr->per;  // Read from race definition

// Instance properties are stored in monster_type
int current_hp = m_ptr->hp;   // Read from instance
int is_stunned = m_ptr->stunned;  // Read from instance
```

### Why This Design?

- **Memory efficiency**: ~1000 monsters on a level; storing race data per monster would be wasteful
- **Dynamic balance**: Changing race definitions via data files affects all instances
- **Clear separation**: Immutable race vs. mutable instance state

---

## Practical Implications

### ✅ What You CAN Do

1. **Modify monster HP**: `m_ptr->hp -= damage` → persists to save
2. **Apply status effects**: `m_ptr->stunned = 5` → persists to save
3. **Change morale/stance**: `m_ptr->morale = -50` → persists to save
4. **Alter alertness**: `set_alertness(m_ptr, value)` → persists to save
5. **Affect spellcasting**: `m_ptr->mana = 0` → persists to save
6. **Apply haste/slow**: `m_ptr->hasted = 10` → persists to save

### ❌ What You CANNOT Do (Permanently)

1. ❌ Increase a monster's **perception/stealth/will** per-instance (race-bound)
2. ❌ Change **damage dice** of monster's attacks (race-bound)
3. ❌ Modify **base HP calculation** for a monster (race-bound)
4. ❌ Alter **racial flags** like RF1_UNIQUE, RF1_SMART, etc. (race-bound)
5. ❌ Change which **spells a monster can cast** (race-bound)

**Workaround for #1**: Apply curses (CUR_MON_PER, CUR_MON_STL, CUR_MON_WIL) which **do** persist and modify skill calculations.

---

## Code References

| File | Function | Purpose |
|------|----------|---------|
| `monster2.c:1169-1218` | `monster_skill()` | Calculate monster skills |
| `monster2.c:1219-1262` | `monster_stat()` | Calculate derived stats |
| `save.c:552-600` | `wr_monster()` | Serialize monster to disk |
| `load.c:551-600` | `rd_monster()` | Deserialize monster from disk |
| `xtra2.c:2681-2684` | Damage application | Modify `m_ptr->hp` |
| `cmd1.c:268+` | `set_alertness()` | Modify `m_ptr->alertness` |
| `monster2.c:2323-2397` | `set_hasted()`, `set_slowed()` | Modify speed effects |

---

## Summary Table: Modifiability vs. Persistence

| Aspect | Can Modify? | Persists to Save? | Scope |
|--------|-------------|-------------------|-------|
| Monster Instance HP | ✅ Yes | ✅ Yes | Current monster |
| Monster Instance Status (stun/confuse) | ✅ Yes | ✅ Yes | Current monster |
| Monster Racial Stats (STR/CON) | ❌ No | N/A | All instances of race |
| Monster Racial Skills (PER/STL/WIL) | ⚠️ Via curses only | ✅ Yes (curse persistence) | All instances of race |
| Monster Morale/Stance | ✅ Yes | ✅ Yes | Current monster |
| Monster Alertness | ✅ Yes | ✅ Yes | Current monster |
| Monster Spellcasting State | ✅ Yes | ✅ Yes | Current monster |
| Monster Haste/Slow | ✅ Yes | ✅ Yes | Current monster |

---

## Conclusion

Monsters in Sil-More follow a **clean separation of concerns**: immutable racial definitions drive static properties (damage, skills, abilities), while modifiable instance properties (hp, status, morale) handle dynamic combat state. This allows the player to affect individual monsters through damage, status effects, and morale changes—all of which **persist across saves**—while preserving the fundamental power level of each race across all instances.

For features requiring permanent, per-instance stat modifications beyond this model, the codebase would need structural changes to either:
1. Store derived stats in `monster_type` (memory cost), or
2. Implement per-instance racial modifiers (complexity cost)
