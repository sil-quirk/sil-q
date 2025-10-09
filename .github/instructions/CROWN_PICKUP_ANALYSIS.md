# Taking the Whole Iron Crown - Analysis

## Can You Pick It Up?

**YES**, you can pick up the Iron Crown and carry it!

### Crown Properties

From `lib/edit/artefact.txt`:
```
N:178:of Morgoth        # ART_MORGOTH_3 (3 Silmarils)
I:33:50:6              # Tval 33 (crown), Sval 50, 6 light radius
W:20:1:4000:10000000   # Depth 20, rarity 1, WEIGHT 4000, value 10M
P:0:0d0:0:0d0:0        # No combat stats
F:INSTA_ART            # Instant artifact (special drop)
```

**Key fact:** Weight = **4000** (40.0 lbs!)

For comparison:
- Normal items: 10-300 weight
- Heavy armor: 200-400 weight  
- This crown: **4000 weight** - it's MASSIVE!

### Restrictions

**What you CAN'T do:**
1. ❌ **Wear it** - Despite being a crown (tval=33), special code prevents wearing:
   ```c
   // in cmd3.c item_tester_hook_wear()
   if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
       return (false);  // Cannot be worn!
   ```

2. ❌ **Destroy it** - The crown is protected from terrain destruction:
   ```c
   // in cave.c cave_valid_bold()
   if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
       return false;  // Can't destroy terrain with crown on it
   ```

**What you CAN do:**
1. ✅ **Pick it up** - No special restrictions on pickup
2. ✅ **Carry it** - It goes in your inventory (takes 1 slot)
3. ✅ **Drop it** - Normal item dropping works
4. ✅ **Destroy it manually** - You can use the 'k' command to destroy it

## Can You Leave the Throne Room With It?

**YES!** There are NO restrictions on leaving Morgoth's level while carrying the crown.

### What Happens

**If you pick up the crown and leave:**

1. **The crown comes with you** - It's just an inventory item
2. **You DON'T get silmarils** - They're still attached to the crown
3. **Morgoth DOESN'T drop another crown** - It's a unique artifact (INSTA_ART flag)
4. **You CAN prise silmarils later** - The crown stays in your inventory, you can use 'k' on it anytime

### Strategic Implications

**Scenario: Grab crown and run**

```
1. Hit Morgoth hard enough to drop crown
2. Pick up entire crown (4000 weight!)
3. Run to stairs
4. Leave Morgoth's level
5. Go up to a safer level (e.g. depth 19)
6. Prise silmarils from crown in safety
7. Drop empty crown
8. Continue escape
```

**Advantages:**
- ✅ Prise silmarils in SAFETY (no Morgoth nearby)
- ✅ No time pressure during prising attempts
- ✅ Can rest between attempts
- ✅ Won't trigger "Morgoth sees" anger mechanics
- ✅ If weapon shatters, you have time to find/smith a new one

**Disadvantages:**
- ❌ Crown weighs 4000! You'll be severely encumbered
- ❌ Need to carry it through pursuit (Morgoth following)
- ❌ Can't prise silmarils while being chased
- ❌ Heavy weight makes combat/fleeing harder
- ❌ Takes up inventory slot

## Weight Impact

With **4000 weight**, the crown alone exceeds most characters' carrying capacity!

**Typical character weight limit:** ~5000-8000 depending on STR
- Carrying crown = ~50-80% of total capacity
- You'll have VERY limited room for other equipment
- Movement speed will be HEAVILY penalized
- Combat will be much harder

## The "Grab and Run" Strategy

**Feasibility: DIFFICULT BUT POSSIBLE**

### Step-by-Step

```
Turn 1: Hit Morgoth for 10+ damage
  → Crown drops at his feet
  
Turn 2: Pick up crown (4000 weight!)
  → Inventory: Crown of Morgoth [3 Silmarils]
  → Weight: HEAVILY ENCUMBERED
  
Turn 3-10: Run to stairs (slow due to weight)
  → Morgoth pursuing (on_the_run = true)
  → Speed heavily penalized
  
Turn 11: Reach stairs, climb up
  → Morgoth anger check: silmarils_possessed() = 0
  → No additional anger (still have crown, not silmarils)
  
Depth 19: Safe on upper level
  → Morgoth still pursuing but not on this level
  → Drop crown on floor
  → Use 'k' to prise silmarils (multiple attempts)
  → Pick up silmarils (much lighter!)
  → Leave crown on floor
  
Continue: Escape with silmarils
  → Next time you leave a level: anger check
  → Morgoth realizes silmarils missing
  → Anger increases based on count
```

### Key Question: Does Morgoth Pursue If You Have Crown?

**Looking at pursuit trigger in `cmd2.c`:**

```c
if (p_ptr->depth == MORGOTH_DEPTH)
{
    if (!p_ptr->morgoth_slain)
    {
        msg_print("As you climb the stair, a great cry of rage and anguish comes from below.");
        msg_print("Make quick your escape: it shall be hard-won.");
    }
    
    p_ptr->on_the_run = true;  // Set regardless of what you're carrying!
}
```

**YES - Morgoth pursues if you leave depth 20, regardless of whether you have crown or silmarils!**

The pursuit is triggered by **leaving his level**, not by what you're carrying.

### Anger Check During Pursuit

From our recent changes in `cmd2.c`:

```c
p_ptr->on_the_run = true;

/* Check silmarils and update Morgoth's anger accordingly */
int sils = silmarils_possessed();
if (sils > 0 && target_state > p_ptr->morgoth_state)
{
    anger_morgoth(1 + sils);
}
```

**Important:** This checks `silmarils_possessed()`, NOT "has crown"!

**So if you leave with JUST the crown (no prised silmarils):**
- `silmarils_possessed() = 0`
- NO additional anger from pursuit check
- You're still at state 1 (crown dropped)

## Practical Analysis

### Is This Worth It?

**Probably NOT, here's why:**

1. **Weight is CRIPPLING**
   - 4000 weight makes you nearly immobile
   - Morgoth will catch up easily
   - You'll struggle to fight back

2. **You still get pursued**
   - Leaving depth 20 triggers pursuit regardless
   - Morgoth doesn't care about the crown, he cares you're fleeing
   - No safety benefit until you're several levels up

3. **Can't prise while pursued**
   - Morgoth keeps showing up on your level
   - Need to keep fleeing
   - Eventually have to prise under pressure anyway

4. **Better strategy:**
   - Prise silmarils while Morgoth is stunned/fleeing
   - Drop crown immediately (weights nothing)
   - Flee with just the silmarils (light weight)
   - Much faster escape

### When It MIGHT Work

**Scenario: Morgoth nearly dead**

```
1. Damage Morgoth to <10% HP
2. He's FLEEING (running away from you)
3. Crown drops
4. You have 5-10 turns before he returns
5. Quick! Grab crown, run to stairs
6. He won't pursue effectively (fleeing)
7. Prise on upper levels at leisure
```

**But even then:**
- The weight penalty makes this risky
- Faster to just prise immediately
- Morgoth might recover and return

## Code Evidence: No Crown-Specific Restrictions

I searched the entire codebase and found:

**Crown special handling:**
- ❌ Cannot be worn (item_tester_hook_wear)
- ❌ Cannot be destroyed by terrain (cave_valid_bold)  
- ❌ Special prise mechanics (do_cmd_destroy → prise_silmaril)

**NO crown-specific handling for:**
- ✅ Pickup (no restrictions)
- ✅ Carrying (normal inventory)
- ✅ Leaving levels (no checks)
- ✅ Dropping (normal dropping)

## Conclusion (UPDATED 2025)

**YES, you CAN take the whole crown** and it's NOW A VIABLE STRATEGY!

### The Crown Theft Balance (New System)

**What triggers anger states:**
1. Crown drops → **State 1**
2. Leave with crown → **State 3** (crown theft!)
3. Leave with prised silmarils (no crown) → State 2/3/4 based on count
4. Prise silmarils later (Morgoth doesn't see) → **No additional anger**
5. Prise in front of Morgoth → **State 4**

### Strategy Comparison

**Option A: Prise Immediately (Traditional)**
- Drop crown when it falls
- Prise 1-3 silmarils (risk weapon shatters)
- If Morgoth sees: State 2/3/4 immediately
- Flee with light silmarils
- **Final anger:** State 2-4 (depending on what you got)

**Option B: Crown Theft (NEW VIABLE!)**
- Pick up entire crown (4000 weight!)
- Leave with crown → **State 3** guaranteed
- Prise silmarils on safer level (no vision check)
- Drop empty crown, flee with silmarils
- **Final anger:** State 3 (unless he catches you prising)

### The Weight Tax

**4000 weight is the price of safety:**
- Severely encumbered during escape
- Slower movement (Morgoth may catch up)
- Harder combat if cornered
- But worth it for safe silmaril extraction!

### Bottom Line

**The crown-grab strategy is NOW SMART**, not stupid!

**Use it if:**
- ✅ You value safety over speed
- ✅ You can handle 4000 weight
- ✅ You want to avoid weapon shatters in front of Morgoth
- ✅ You're okay with guaranteed State 3

**Avoid it if:**
- ❌ Your STR is too low (can't carry 4000)
- ❌ You need fast escape (weight slows you down)
- ❌ You already damaged Morgoth to State 3+ (no benefit)

The game ALLOWS it and NOW PROPERLY PUNISHES/REWARDS it! It's a valid high-risk, high-reward strategy. 💎👑
