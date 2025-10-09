# Morgoth Anger System - Bug Analysis and Fix

## Problem Report
The Morgoth anger system was not working correctly:
1. After dropping the crown and praising silmarils, Morgoth remained in mode 0 (+20 6d10) instead of advancing to higher modes
2. State was not applied correctly when loading saved games
3. Taking the first silmaril did not trigger state 2

## Root Cause Analysis - THREE Critical Bugs Found

### Issue 1: Save/Load Bug (CRITICAL - FIXED)
**Problem:** The `anger_morgoth()` function modifies the `r_info[R_IDX_MORGOTH]` race template directly. When a game is saved, only `p_ptr->morgoth_state` is saved. When the game is loaded:
1. `p_ptr->morgoth_state` is correctly loaded from the save file
2. BUT the `r_info` template is reset to default values (from `lib/edit/monster.txt`)
3. The modified stats are never re-applied!

**Result:** Loading a saved game with `morgoth_state > 0` would reset Morgoth's stats to mode 0, even though the state variable said otherwise.

**Fix:** Added code in `load.c` to reapply the Morgoth state after loading:
```c
/* Reapply Morgoth's anger state to the r_info template */
if (p_ptr->morgoth_state > 0)
{
    log_debug("load: reapplying morgoth_state %d to r_info template", 
             p_ptr->morgoth_state);
    
    /* Save current state, then reset to 0 and reapply */
    s16b saved_state = p_ptr->morgoth_state;
    p_ptr->morgoth_state = 0;
    anger_morgoth(saved_state);
}
```

### Issue 2: Non-Cumulative State Changes (CRITICAL - FIXED)

**Problem:** The original `anger_morgoth()` used a switch statement that only applied changes specific to each state level. This meant that when transitioning directly from state 1 to state 3 (which happens when taking the 2nd silmaril), state 2's attack upgrades were **skipped**!

**Example of the Bug:**
- State 1: Sets evn=22, light=0, per=15 (but NOT attack)
- State 2: Sets att=30, dd=7, wil=30, per=20, evn=25 ← **SKIPPED when going 1→3**
- State 3: Sets pd=7, wil=35, per=25 (but NOT attack or dice!)

So taking 2 silmarils would leave Morgoth with:
- ❌ Attack: 20 (should be 30 from state 2)
- ❌ Dice: 6d10 (should be 7d10 from state 2)
- ✅ Protection: 7 (correct from state 3)
- ✅ Will: 35 (correct from state 3)

**Fix:** Changed from switch statement to cumulative if-statements that apply ALL changes up to the target level:
```c
/* Apply all changes cumulatively up to the target level */
/* This ensures stats are correct even when skipping intermediate states */

/* State 0: Base values */
if (level >= 0) { /* reset to base */ }

/* State 1: Crown lost */
if (level >= 1) { /* apply state 1 changes */ }

/* State 2: Hurt or 1st Silmaril stolen */
if (level >= 2) { /* apply state 2 changes */ }

/* State 3: Badly hurt or 2nd Silmaril */
if (level >= 3) { /* apply state 3 changes */ }

/* State 4: Desperate or 3rd Silmaril */
if (level >= 4) { /* apply state 4 changes */ }
```

Now when going from state 1→3, it applies:
1. State 0: Reset to base
2. State 1: Crown changes
3. State 2: **Attack and dice upgrade** ✅
4. State 3: Protection and will upgrade ✅

**Result:** All stats are correctly applied regardless of which states are skipped!

### Issue 3: Missing 1st Silmaril Trigger (CRITICAL - FIXED)

**Problem:** The `prise_silmaril()` function had logic to anger Morgoth for the 2nd and 3rd silmarils, but the **1st silmaril case was completely empty**!

```c
switch (o_ptr->name1)
{
case ART_MORGOTH_3:  // 1st silmaril
{
    break;  // ← EMPTY! No anger_morgoth() call!
}
case ART_MORGOTH_2:  // 2nd silmaril
{
    // ... anger_morgoth(3) called here
}
```

**Result:** Taking the 1st silmaril in front of Morgoth would NOT trigger state 2! The player would have to rely on HP damage to trigger the state transition instead.

**Fix:** Added the missing anger logic for the 1st silmaril:
```c
case ART_MORGOTH_3:  // 1st silmaril
{
    /* Process monsters - anger Morgoth when 1st Silmaril is taken */
    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        
        if (m_ptr->r_idx == R_IDX_MORGOTH
            && m_ptr->alertness >= ALERTNESS_ALERT)
        {
            if ((m_ptr->cdis <= 5)
                && los(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx))
            {
                msg_print("Morgoth roars in fury!");
                anger_morgoth(2);  // State 2 for 1st silmaril
            }
        }
    }
    break;
}
```

**Now the progression is correct:**
- Crown drops → State 1
- 1st Silmaril taken (if Morgoth sees) → State 2 ✅
- 2nd Silmaril taken (if Morgoth sees) → State 3
- 3rd Silmaril taken → State 4

### Issue 2: State Transition Logic Requirements
- Triggered in `drop_iron_crown()` when crown is created and dropped
- No vision/distance requirements
- Always triggers when crown drops

#### 1st Silmaril Taken (State ? → 2)
- Triggered in `prise_silmaril()` when ART_MORGOTH_3 is successfully pried
- **Requirements:**
  - Morgoth must be found in monster list
  - Morgoth must be `ALERTNESS_ALERT` or higher
  - Morgoth must be within 5 squares (`cdis <= 5`)
  - Line of sight to player
- **Message:** "Morgoth roars in fury!"

#### 2nd Silmaril Taken (State ? → 3)
- Triggered in `prise_silmaril()` when ART_MORGOTH_2 is successfully pried
- **Requirements:**
  - Morgoth must be found in monster list
  - Morgoth must be `ALERTNESS_ALERT` or higher
  - Morgoth must be within 5 squares (`cdis <= 5`)
  - Line of sight to player
- **Message:** "Morgoth howls with rage!"

#### 3rd Silmaril Taken (State ? → 4)
- Triggered when ART_MORGOTH_0 is successfully pried (after crown changes)
- No vision/distance requirements
- **Message:** "You hear a cry of vengeance echo through the iron hells."

#### HP-Based Transitions
Additional transitions in `melee2.c process_monster()`:
- **WOUNDED** (< 75% HP): → State 2 (if current < 2)
  - Message: "Morgoth grows angry."
- **BADLY_WOUNDED** (< 50% HP): → State 3 (if current < 3)
  - Message: "Morgoth unslings his mighty shield."
- **ALMOST_DEAD** (< 25% HP): → State 4 (if current < 4)
  - Message: "Morgoth grows desperate."

## State Definitions (Cumulative)

**Important:** States are now cumulative - each level includes all previous changes!

```c
State 0: Initial (base values from monster.txt)
    evn = 20
    att = 20
    dd = 6d10
    pd = 5
    wil = 25
    per = 10
    light = 7

State 1: Crown lost (cumulative from State 0)
    evn = 22      ← changed
    att = 20
    dd = 6d10
    pd = 5
    wil = 25
    per = 15      ← changed
    light = 0     ← changed

State 2: Hurt or 1st Sil (cumulative from State 1)
    evn = 25      ← changed
    att = 30      ← changed
    dd = 7d10     ← changed
    pd = 5
    wil = 30      ← changed
    per = 20      ← changed
    light = 0

State 3: Badly hurt or 2nd Sil (cumulative from State 2)
    evn = 25
    att = 30      ← from state 2
    dd = 7d10     ← from state 2
    pd = 7        ← changed
    wil = 35      ← changed
    per = 25      ← changed
    light = 0

State 4: Desperate or 3rd Sil (cumulative from State 3)
    evn = 30      ← changed
    att = 40      ← changed
    dd = 8d10     ← changed
    pd = 7
    wil = 40      ← changed
    per = 30      ← changed
    light = 0
```

## Debugging Added

### Comprehensive Logging
Added `log_debug()` calls throughout the system to trace:

1. **`anger_morgoth()` in `xtra2.c`:**
   - Entry point with requested level and current state
   - Before/after stats (att, dd, ds, evn, pd, wil, per, light)
   - State transition confirmation

2. **`drop_iron_crown()` in `cmd1.c`:**
   - Crown artifact check
   - Drop location
   - anger_morgoth(1) call

3. **`prise_silmaril()` in `cmd3.c`:**
   - Entry with artifact ID
   - Current morgoth_state and silmarils_possessed
   - Success/failure for each silmaril
   - Morgoth location and visibility checks
   - Final state

4. **`shatter_weapon()` in `cmd3.c`:**
   - Silmaril number
   - Calculated anger level
   - Morgoth location and visibility

5. **`process_monster()` in `melee2.c`:**
   - HP-based transitions with current/max HP values

6. **`do_cmd_go_up()` in `cmd2.c`:**
   - **Pursuit anger check:** When leaving Morgoth's level (setting `on_the_run = true`), checks silmaril count ONCE and updates anger accordingly

7. **`rd_savefile_new_aux()` in `load.c`:**
   - Loaded morgoth_state value
   - Reapplication of state to r_info template

## Pursuit/Escape Anger System

**Key Rule:** Anger NEVER decreases - it's a one-way ratchet!

When you leave Morgoth's level and start the escape (`on_the_run = true`), the game checks **ONCE** how many silmarils you have and updates Morgoth's anger accordingly. This happens in `do_cmd_go_up()` in `cmd2.c`.

```
silmarils_possessed() = 0 → State 1 (just crown lost)
silmarils_possessed() = 1 → State 2 (if not already higher)
silmarils_possessed() = 2 → State 3 (if not already higher)
silmarils_possessed() = 3 → State 4 (if not already higher)
```

**This handles two scenarios:**
1. **Stealth theft:** You took silmarils when Morgoth wasn't alert/nearby/watching
2. **Quick escape:** You grabbed silmarils and ran before Morgoth could react

**Why only once?** You can't get more silmarils after leaving Morgoth's level - they're all on level 20! The check happens once when `p_ptr->on_the_run` is first set to true.

**Messages during pursuit:**
- 1st Silmaril: "Morgoth roars with rage as he realizes a Silmaril is missing!"
- 2nd Silmaril: "Morgoth howls in fury - another Silmaril stolen!"
- 3rd Silmaril: "Morgoth's wrath is terrible - all Silmarils are gone!"

**Note:** If HP damage or direct silmaril theft already put Morgoth at state 4, the pursuit check won't decrease it. Anger only goes UP, never DOWN.

## How to Debug

1. **Check log files:**
   - SDL3 build: `sil-more-windows-sdl3/log.txt`
   - Win32 build: `log.txt` in repo root or `src/`

2. **Search for morgoth-related entries:**
   ```
   grep -i morgoth log.txt
   ```

3. **Key log messages to look for:**
   - `anger_morgoth: called with level=X`
   - `anger_morgoth: transitioning from state X to state Y`
   - `anger_morgoth: BEFORE/AFTER` (shows stat changes)
   - `drop_iron_crown: calling anger_morgoth(1)`
   - `prise_silmaril: Morgoth sees Xth silmaril taken`
   - `process_monster: Morgoth WOUNDED/BADLY_WOUNDED/ALMOST_DEAD`
   - `load: morgoth_state loaded as X`
   - `load: reapplying morgoth_state X to r_info template`

## Testing Checklist

To verify the fix works:

1. **Save/Load Test:**
   - [ ] Start new game, reach Morgoth
   - [ ] Drop his crown (should see state 0 → 1)
   - [ ] Save game
   - [ ] Load game
   - [ ] Check log: should see "reapplying morgoth_state 1"
   - [ ] Verify Morgoth's stats match state 1 (evn=22, light=0, per=15)

2. **Silmaril Test:**
   - [ ] Prise first silmaril while Morgoth is alert and nearby
   - [ ] Check if state advances (depends on HP or 2nd silmaril)
   - [ ] Prise second silmaril with Morgoth alert, <5 squares, LOS
   - [ ] Should see "Morgoth howls with rage!" and state → 3
   - [ ] Prise third silmaril
   - [ ] Should see state → 4

3. **HP Test:**
   - [ ] Damage Morgoth to <75% HP
   - [ ] Should see "Morgoth grows angry" and state → 2
   - [ ] Damage to <50% HP
   - [ ] Should see "Morgoth unslings his mighty shield" and state → 3
   - [ ] Damage to <25% HP
   - [ ] Should see "Morgoth grows desperate" and state → 4

## Files Modified

1. `src/xtra2.c` - Enhanced anger_morgoth() with logging
2. `src/cmd1.c` - Added logging to drop_iron_crown()
3. `src/cmd3.c` - Added logging to prise_silmaril() and shatter_weapon()
4. `src/melee2.c` - Added logging to HP-based transitions
5. `src/load.c` - Added logging and **FIX** to reapply state on load

## Date
2025-10-09
