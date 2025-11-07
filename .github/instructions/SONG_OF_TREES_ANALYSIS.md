# Song of Trees & GF_LIGHT - Technical Analysis# Song of Trees & GF_LIGHT - Technical Analysis



## Implementation## Implementation



### Song of Trees### Song of Trees

```c```c

void sing_song_of_trees(int score)void sing_song_of_trees(int score)

{{

    int rad = 1 + (score / 5);    int rad = 1 + (score / 5);

    int dd = 1 + (score / 10);    int dd = 1 + (score / 10);

    int ds = score;    int ds = score;

    light_area(dd, ds, rad);  // GF_LIGHT attack    light_area(dd, ds, rad);  // GF_LIGHT attack

}}

``````



### GF_LIGHT Handler### GF_LIGHT Handler



**ONLY affects HURT_LITE monsters** ("Hurt by light!")**ONLY affects HURT_LITE monsters** ("Hurt by light!")



For HURT_LITE monsters:**For HURT_LITE monsters:**

1. **Always:** Stunned ("cringes from the light!")1. **Always:** Stunned

2. **If light > 2 and player-caused:** Will-based damage2. **If light > 2 and player-caused:** Will-based damage



For non-HURT_LITE monsters: **No effect whatsoever****For non-HURT_LITE monsters:** No effect whatsoever



**Skill determination:****Skill determination:**

- If `ds > 10`: Use `ds` as skill (Song of Trees passes song_score)- If `ds > 10`: Use `ds` as skill (Song of Trees)

- If `ds ≤ 10`: Use player Will (Gem/Staff of Light passes 4)- If `ds ≤ 10`: Use player Will (Gem/Staff of Light)



------



## Light Mechanics## Light Mechanics



### Light Sources### Light Level Calculation



| Source | Radius | Light at d=0 | d=1 | d=2 | d=3 |`cave_light[y][x]` formula:

|--------|--------|--------------|-----|-----|-----|```

| Torch | 1 | 2❌ | 1❌ | - | - |light_value = BASE + PLAYER_LIGHT + MONSTER_LIGHT + OBJECT_LIGHT

| Lantern | 2 | 3✓ | 2❌ | 1❌ | - |```

| Mallorn | 3 | 4✓ | 3✓ | 2❌ | 1❌ |

| Fëanorian | 4 | 5✓ | 4✓ | 3✓ | 2❌ |**Player Light at distance d:**

| Silmaril | 7 | 8✓ | 7✓ | 6✓ | 5✓ |```

light_value = (player_radius + 1 - d) + bonuses

❌ = light ≤ 2 (no damage)  ```

✓ = light > 2 (damage possible)

### Light Sources

**Song bonus:** +`score/5` to radius

| Light Source | Radius | Light at d=0 | d=1 | d=2 | d=3 | d=4 |

### With Song Active|--------------|--------|--------------|-----|-----|-----|-----|

| Torch | 1 | 2 | 1 | - | - | - |

| Song+Lantern | Total Radius | Can Damage At || Lantern | 2 | 3 | 2 | 1 | - | - |

|--------------|--------------|---------------|| Mallorn | 3 | 4 | 3 | 2 | 1 | - |

| 5 + Lantern | 3 | Same square only || Fëanorian | 4 | 5 | 4 | 3 | 2 | 1 |

| 10 + Lantern | 4 | 1 square || Silmaril | 7 | 8 | 7 | 6 | 5 | 4 |

| 15 + Lantern | 5 | 2 squares |

| 20 + Lantern | 6 | 3 squares |**Bonuses:**

- Inner Light: +2 at all squares

---- Sunlight terrain: +3 at player square

- Song of Trees: +`score/5` to radius

## HURT_LITE Monsters- Glowing weapons: +1 radius each



Only these monsters are affected by GF_LIGHT:### Critical Threshold: Light > 2



| Monster | Will | HP (avg) | Depth |Damage only triggers when `cave_light ≥ 3` at monster's position.

|---------|------|----------|-------|

| Orc Thrallmaster | 2 | 15 | 1 |**Maximum damage distance by light source:**

| Orc Scout | 1 | 15 | 3 |

| Orc Warrior | 2 | 18 | 3 || Light Source | Can damage up to |

| Orc Captain | 2 | 24 | 5 ||--------------|------------------|

| Mountain Troll | 2 | 35 | 7 || Torch | Never (max light = 2) |

| Tattered Wight | 5 | 10 | 7 || Lantern | Same square only |

| Orc Champion | 5 | 28 | 8 || Mallorn | 1 square away |

| Boldog (unique) | 6 | 33 | 7 || Fëanorian | 2 squares away |

| Cave Troll | 2 | 46 | 10 || Silmaril | 5 squares away |



**Immune monsters include:** Easterlings, most undead, wolves, serpents, spiders, dragons, etc.**With Song of Trees active:**



---| Song+Lantern | Total Radius | Max Damage Range |

|--------------|--------------|------------------|

## Damage Formula| Song 5 | 3 | 0 (same square) |

| Song 10 | 4 | 1 square |

```| Song 15 | 5 | 2 squares |

dd = 1 + (score / 10)| Song 20 | 6 | 3 squares |

base_damage = damroll(dd, light_level)

will_result = skill_check(skill, monster_will + distance)---

final_damage = (base × will_result) / (will_result + 5)

```## Damage Calculations



**Requirements:**### Formula:

1. Monster must have HURT_LITE flag

2. Light level > 2 at monster position  ```

3. Player-caused attack1. dd = 1 + (song_score / 10)

4. Monster fails Will save2. Base damage = damroll(dd, light_level)

3. Will check: result = skill_check(skill, monster_will + distance)

### Will Reduction4. If result > 0: final = (base × result) / (result + 5)

5. If result ≤ 0: final = 0

| Result | Multiplier |```

|--------|-----------|

| +20 | 80% |### Damage Reduction by Will Save:

| +10 | 67% |

| +5 | 50% || Will Result | Damage Multiplier |

| +1 | 17% ||-------------|-------------------|

| ≤0 | 0% || +20 | 80% |

| +15 | 75% |

---| +10 | 67% |

| +5 | 50% |

## Examples| +3 | 38% |

| +1 | 17% |

### Orc Thrallmaster (Will 2, HP 15)| ≤0 | 0% |

**Song 10, Lantern (r=4), distance 1:**

- Light: 3---

- Damage: 2d3 (avg 4)

- Will: d10+10 vs d10+3 → +7## Example Calculations

- Final: **2.9/round**

- **Stunned**---

- ~5 rounds to kill

## HURT_LITE Monsters

### Mountain Troll (Will 2, HP 35)

**Song 15, Lantern+Song (r=5), distance 2:**Monsters affected by GF_LIGHT attacks:

- Light: 4

- Damage: 2d4 (avg 5)| Monster | Will | HP (avg) | Depth | Notes |

- Will: d10+15 vs d10+4 → +11|---------|------|----------|-------|-------|

- Final: **3.4/round**| Orc Thrallmaster | 2 | 15 | 1 | Early orc |

- **Stunned**| Orc Scout | 1 | 15 | 3 | Scouts ahead |

- ~10 rounds to kill| Orc Warrior | 2 | 18 | 3 | Basic orc |

| Orc Captain | 2 | 24 | 5 | Orc leader |

### Tattered Wight (Will 5, HP 10)| Mountain Troll | 2 | 35 | 7 | Slow, tough |

**Song 15, Lantern+Song (r=5), distance 1:**| Tattered Wight | 5 | 10 | 7 | Undead |

- Light: 5| Orc Champion | 5 | 28 | 8 | Elite orc |

- Damage: 2d5 (avg 6)| Boldog | 6 | 33 | 7 | Named orc |

- Will: d10+15 vs d10+6 → +9| Cave Troll | 2 | 46 | 10 | Very tough |

- Final: **3.9/round**

- **Stunned****Note:** Non-HURT_LITE monsters (Easterlings, most undead, animals, etc.) are completely immune to GF_LIGHT effects.

- ~3 rounds to kill

---

### Orc Champion (Will 5, HP 28)**Setup:** Song 10, Lantern (total radius 4), distance 1

**Song 20, Fëanorian (r=8), distance 2:**

- Light: 7- Light at distance 1: 4

- Damage: 3d7 (avg 12)- Damage dice: 2d4 (avg 5)

- Will: d10+20 vs d10+7 → +13- Will check: d10+10 vs d10+2+1 → avg result = +7

- Final: **8.7/round**- Final damage: (5 × 7) / 12 = **2.9 damage/round**

- **Stunned**- **Time to kill: ~5 rounds**

- ~3 rounds to kill- **Bonus:** Also gets stunned (HURT_LITE)



---#### Scenario 2: Mountain Troll (Will 2, HP 35, HURT_LITE)

**Setup:** Song 15, Lantern+Song (total radius 5), distance 2

## Gem/Staff of Light

- Light at distance 2: 6 - 2 = 4

```c- Damage dice: 2d4 (avg 5)

light_area(4, 4, 7)  // dd=4, ds=4, rad=7- Will check: d10+15 vs d10+2+2 → avg result = +11

```- Final damage: (5 × 11) / 16 = **3.4 damage/round**

- **Time to kill: ~10 rounds**

**Uses player Will** (ds=4 ≤ 10)  - **Bonus:** Stunned every round (HURT_LITE)

**Only affects HURT_LITE monsters** (same as Song)

#### Scenario 3: Easterling Warrior (Will 7, HP 25, NO HURT_LITE)

### Comparison**Setup:** Song 20, Fëanorian (radius 8), distance 2



| | Song | Gem |- Light at distance 2: 9 - 2 = 7

|-|------|-----|- Damage dice: 3d7 (avg 12)

| Skill | Song (5-25) | Will (0-20+) |- Will check: d10+20 vs d10+7+2 → avg result = +11

| Damage | (1+score/10)dX | 4dX |- Final damage: (12 × 11) / 16 = **8.25 damage/round**

| Radius | 1+(score/5) | 7 |- **Time to kill: ~3 rounds**

| Frequency | Every round | Per charge |- **Proves it works on non-HURT_LITE monsters!**

| Light bonus | +score/5 | None |

| Targets | HURT_LITE only | HURT_LITE only |---



### Example: Gem vs Orc (Will 2, HURT_LITE)## Part 5: Gem/Staff of Light Analysis

**Player Will 12, Silmaril, distance 3:**

- Light: 5### How It Differs from Song of Trees:

- Damage: 4d5 (avg 12)

- Will: d10+12 vs d10+5 → +7**Gem/Staff of Light:**

- Final: **7 damage**```c

- **Stunned**light_area(4, 4, 7)  // dd=4, ds=4, rad=7

```

---

**Detection Logic in GF_LIGHT handler:**

## Tactical Guide```c

if (ds > 10) {

### Target Selection    skill_to_use = ds;           // Song of Trees (ds = song_score)

**Primary targets:** Orcs and trolls (most have HURT_LITE)  } else {

**Avoid:** Easterlings, wolves, serpents, spiders (no HURT_LITE)    skill_to_use = p_ptr->skill_use[S_WIL];  // Gem/Staff (ds = 4)

}

### Early Game (Song 5-10)```

- **Problem:** Torch insufficient

- **Solution:** Get Lantern### Gem of Light Damage:

- **Use:** Melee range supplement vs orcs

**Same requirements:**

### Mid Game (Song 15)  - Light level > 2 at monster position

- Lantern/Mallorn- Monster must fail Will save

- 1-2 square range- Player-caused attack

- 2-4 damage/round

- Good for orc packs in hallways**Different calculation:**

- Uses **player's Will skill** instead of Song score

### Late Game (Song 20+)- Fixed damage dice: **4dX** where X = light_level

- Fëanorian/Silmaril- Same Will resistance formula

- 3-5+ square range

- 6-12 damage/round### Example: Gem of Light vs Orc (Will 2)

- Excellent vs orc/troll swarms

**Player with Will 10, using Gem, monster at distance 3 from player:**

---

Assuming Fëanorian Lamp (radius 4):

## Key Points- Light at distance 3: 5 - 3 = 2 ❌ **NO DAMAGE** (light ≤ 2)



✅ **Uses GF_LIGHT projectile system**  With Silmaril (radius 7):

✅ **ONLY affects HURT_LITE monsters**  - Light at distance 3: 8 - 3 = 5 ✓

✅ **Primary targets:** Orcs, trolls, some undead  - Damage dice: 4d5 (avg 12)

✅ **Immune:** Easterlings, animals, most monsters  - Will check: d10+10 vs d10+2+3 → avg result = +5

✅ **Gated by:** HURT_LITE flag, light level (>2), Will save  - Final damage: (12 × 5) / 10 = **6 damage**

✅ **Gem/Staff:** Same rules, uses player Will instead of Song  

✅ **All HURT_LITE monsters are stunned** regardless of damage### Gem vs Song Comparison:


| Aspect | Song of Trees | Gem/Staff of Light |
|--------|---------------|-------------------|
| Skill used | Song score (5-25) | Player Will (0-20+) |
| Damage dice | (1 + score/10)dX | 4dX |
| Radius | 1 + (score/5) | Fixed 7 |
| Frequency | Every round | Per charge |
| Light bonus | Yes (+score/5) | No |

**Trade-offs:**
- **Gem:** Higher base damage (4d), larger radius (7), but limited charges
- **Song:** Scalable damage, continuous effect, boosts light radius, uses voice

---

## Part 6: Will Resistance Deep Dive

### Damage Reduction Formula:

```
final_damage = (base_damage × result) / (result + 5)
```

| Will Save Result | Damage Multiplier |
|------------------|-------------------|
| +20 | 80% (20/25) |
| +15 | 75% (15/20) |
| +10 | 67% (10/15) |
| +5 | 50% (5/10) |
| +3 | 37.5% (3/8) |
| +1 | 16.7% (1/6) |
| 0 or less | 0% |

### Expected Results by Monster Will:

**Against Song 15 at distance 1:**

| Monster Will | Difficulty | Avg Result | Success Rate | Avg Multiplier |
|--------------|------------|------------|--------------|----------------|
| 1 (Orc Scout) | d10+2 | +13 | ~95% | 72% |
| 2 (Thrallmaster) | d10+3 | +12 | ~90% | 71% |
| 5 (Wight) | d10+6 | +9 | ~75% | 64% |
| 7 (Easterling) | d10+8 | +7 | ~65% | 58% |
| 10 (High Will) | d10+11 | +4 | ~50% | 44% |

---

## Part 7: Tactical Analysis

### Early Game (Song 5-10, Torch/Lantern)

**Reality:**
- Torch alone: Ineffective (light never > 2)
- Lantern: Melee range only
- Song 5 + Lantern: Can damage at distance 1
- Low damage (1-2d3-4)

**Best Use:** Passive damage while fighting in melee

### Mid Game (Song 15, Lantern/Mallorn)

- Effective radius: 3-4 squares
- Can damage 1-2 squares away
- Damage: 2d3-5 (avg 4-6 before Will)
- Light radius bonus makes exploration easier

**Best Use:** Hallway fighting, kiting low-Will enemies

### Late Game (Song 20-25, Fëanorian/Silmaril)

- Effective radius: 5-6+ squares
- Can damage 3-5+ squares away
- Damage: 3d5-8 (avg 9-13 before Will)
- Significant AoE pressure

**Best Use:** Room combat, swarm control, continuous pressure

---

## Part 8: GF_LIGHT System Integration

### How the Projectile System Works:

1. `sing_song_of_trees(score)` called every round
2. Calls `light_area(dd, ds, rad)`
3. `light_area()` calls `project()` with GF_LIGHT type
4. `project()` iterates through affected squares
5. For each monster hit: `project_m()` → `case GF_LIGHT:`
6. GF_LIGHT handler:
   - Checks HURT_LITE → stuns if applicable
   - Checks light level > 2 → attempts damage
   - Distinguishes Song vs Will-based source
   - Rolls damage, checks Will, applies reduction

### Advantages of Using GF_LIGHT:

✅ **Proper projectile visualization** - uses game's light effects
✅ **Respects line of sight** - won't hit through walls
✅ **Integrates with existing systems** - projection flags, etc.
✅ **HURT_LITE bonus** - still stuns light-sensitive monsters
✅ **Extensible** - other items can use same system (Gem of Light)
✅ **Performance** - uses optimized projection code

### Other GF_LIGHT Sources:

| Source | dd | ds | Skill Used | Radius |
|--------|----|----|------------|--------|
| Song of Trees 10 | 2 | 10 | Song (10) | 3 |
| Song of Trees 20 | 3 | 20 | Song (20) | 5 |
| Gem of Light | 4 | 4 | Will | 7 |
| Staff of Light | 4 | 4 | Will | 7 |

---

## Part 9: Summary & Balance Assessment

### Current Implementation:

✅ **Uses GF_LIGHT** - proper projectile system
✅ **Works on all monsters** - not limited to HURT_LITE
✅ **Will-based resistance** - scales with monster difficulty
✅ **Light-level gating** - creates tactical light management
✅ **Distinguishes sources** - Song vs Will appropriately
✅ **Preserves HURT_LITE** - bonus stunning on susceptible enemies

### Key Balancing Factors:

**Three-gate system keeps it balanced:**
1. **Light Level Gate** - requires good light source + positioning
2. **Will Save Gate** - high-Will monsters resist well
3. **Distance Penalty** - harder to affect distant monsters

### Power Curves:

**Song of Trees:**
- Weak early (needs good light source)
- Moderate mid (2-3d damage at 1-2 range)
- Strong late (3d damage at 3-5 range)

**Gem of Light:**
- Consistent 4dX damage
- Limited by charges
- Independent of Song investment
- Requires high light level to be effective

### Recommended Builds:

**For Song Focus:**
- Invest in Song 15-20
- Prioritize Lantern → Mallorn → Fëanorian
- Synergizes with Inner Light ability (+2 light)
- Best for hallway/corridor combat

**For Gem Focus:**
- Invest in Will
- Use Silmaril or Fëanorian for max coverage
- Save charges for tough encounters
- Best for burst AoE damage

---

## Conclusion

The Song of Trees implementation using **GF_LIGHT** provides:
- Mechanically sound integration with existing systems
- Balanced damage gated by light, Will, and distance
- Interesting tactical choices around light management
- Works as intended on all monster types
- Distinguishes appropriately between Song and Will-based attacks
