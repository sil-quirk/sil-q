# Original Sil-Q vs Sil-More: Morgoth Anger System Analysis

## Summary of Findings

### Original Sil-Q Implementation (BUGGY)

**The original Sil-Q had the EXACT SAME BUGS we just fixed!**

#### 1. Non-Cumulative States (BUG)
```c
void anger_morgoth(int level)
{
    if (p_ptr->morgoth_state >= level)
        return;

    switch (level)  // ← SWITCH STATEMENT - not cumulative!
    {
    case 1:
        r_info[R_IDX_MORGOTH].evn = 22;
        r_info[R_IDX_MORGOTH].light = 0;
        r_info[R_IDX_MORGOTH].per = 15;
        break;
    case 2:
        r_info[R_IDX_MORGOTH].blow[0].att = 30;  // ← SKIPPED if going 1→3!
        r_info[R_IDX_MORGOTH].blow[0].dd = 7;
        // ...
        break;
    case 3:
        r_info[R_IDX_MORGOTH].pd = 7;
        // ...
        break;
    }
}
```

**Result:** Same bug - jumping from state 1→3 would skip state 2's attack upgrades!

#### 2. Missing 1st Silmaril Trigger (BUG)
```c
switch (o_ptr->name1)
{
case ART_MORGOTH_3:  // 1st silmaril
{
    noise = 5;
    freed_msg = "You have freed a Silmaril!";
    break;  // ← NO anger_morgoth() call!
}
case ART_MORGOTH_2:  // 2nd silmaril
{
    // ... later has anger_morgoth(2) with vision check
}
```

**Result:** Taking the 1st silmaril in front of Morgoth did NOT anger him! (Unless weapon shattered)

#### 3. Wrong Final Silmaril State (BUG)
```c
// check for taking of final Silmaril
if (o_ptr->name1 == ART_MORGOTH_0)
{
    msg_print("You hear a cry of vengeance echo through the iron hells.");
    msg_print("You feel your doom awaiting you.");
    wake_all_monsters(0);
    anger_morgoth(2);  // ← Should be anger_morgoth(4)!
}
```

**Result:** Taking the 3rd silmaril only set state to 2, not 4!

#### 4. No Pursuit Anger Check (MISSING FEATURE)
```c
// In cmd2.c when leaving Morgoth's level:
p_ptr->on_the_run = TRUE;
p_ptr->truce = FALSE;
// ← NO silmaril check here!
```

**Result:** If you took silmarils stealthily (Morgoth not alert/nearby), he would NEVER become angry based on silmarils unless you damaged his HP!

#### 5. No Save/Load State Reapplication (BUG)
- No code to reapply `p_ptr->morgoth_state` to `r_info` template after loading
- Same bug we fixed - loading would reset Morgoth's stats to default

## What Original Sil-Q Got Right

1. ✅ HP-based anger triggers (wounded, badly wounded, almost dead)
2. ✅ Crown drop triggers state 1
3. ✅ Anger never decreases (the `if (morgoth_state >= level) return` check)
4. ✅ Vision/distance checks for 2nd silmaril (alert, within 5 squares, LOS)

## What We Fixed in Sil-More

### 1. Cumulative State Application ✅
```c
// Now uses if-statements instead of switch
if (level >= 0) { /* reset to base */ }
if (level >= 1) { /* state 1 changes */ }
if (level >= 2) { /* state 2 changes */ }  ← Always runs if going to state 3+
if (level >= 3) { /* state 3 changes */ }
if (level >= 4) { /* state 4 changes */ }
```

### 2. Added 1st Silmaril Trigger ✅
```c
case ART_MORGOTH_3:  // 1st silmaril
{
    // Check if Morgoth is alert, nearby, has LOS
    if (conditions_met) {
        msg_print("Morgoth roars in fury!");
        anger_morgoth(2);  // ← NOW TRIGGERS!
    }
}
```

### 3. Fixed Final Silmaril State ✅
```c
if (o_ptr->name1 == ART_MORGOTH_0)
{
    msg_print("You hear a cry of vengeance echo through the iron hells.");
    anger_morgoth(4);  // ← Now correctly goes to state 4!
}
```

### 4. Added Pursuit Anger Check ✅
```c
// In cmd2.c when leaving Morgoth's level:
p_ptr->on_the_run = true;

// NEW: Check silmarils and update anger accordingly
int sils = silmarils_possessed();
if (sils > 0 && (1 + sils) > p_ptr->morgoth_state) {
    anger_morgoth(1 + sils);  // 1 sil → state 2, 2 sils → state 3, 3 sils → state 4
}
```

### 5. Added Save/Load State Reapplication ✅
```c
// In load.c after loading character:
if (p_ptr->morgoth_state > 0)
{
    s16b saved_state = p_ptr->morgoth_state;
    p_ptr->morgoth_state = 0;
    anger_morgoth(saved_state);  // Reapply to r_info template
}
```

## Expected Behavior Comparison

### Original Sil-Q (Buggy)
- Drop crown → State 1 ✅
- Take 1st silmaril in front of Morgoth → **No anger** ❌
- Take 2nd silmaril in front of Morgoth → State 2 (not 3!) ❌
- Take 3rd silmaril → State 2 ❌
- Leave level with stolen silmarils → **No anger** ❌
- HP damage → States 2/3/4 ✅
- Load saved game → **Stats reset** ❌

### Sil-More (Fixed)
- Drop crown → State 1 ✅
- Take 1st silmaril in front of Morgoth → State 2 ✅
- Take 2nd silmaril in front of Morgoth → State 3 ✅
- Take 3rd silmaril → State 4 ✅
- Leave level with stolen silmarils → Anger updated to match silmaril count ✅
- HP damage → States 2/3/4 ✅
- Load saved game → **Stats correctly reapplied** ✅

## Conclusion

The original Sil-Q Morgoth anger system was **fundamentally broken**:
1. Non-cumulative states meant attack never upgraded when taking silmarils
2. First silmaril did nothing
3. Third silmaril only went to state 2 instead of 4
4. Stealth theft was never punished
5. Save/load broke the system

**All these bugs are now fixed in Sil-More!** The system now works as originally intended.
