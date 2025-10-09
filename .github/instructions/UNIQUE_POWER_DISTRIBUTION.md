# Unique Monster Power Distribution Summary

## Quick Reference: Unique Power by Depth

| Depth | Unique Name | Type | HP | Speed | Evasion | Attack | Damage | Will | Notable Features |
|-------|-------------|------|-----|-------|---------|--------|--------|------|------------------|
| 5 | Gorgol | Orc | 12d4 | 2 | +4 | +6 | 3d7 | 5 | RALLY, ESCORT |
| 7 | Boldog | Orc | 13d4 | 2 | +6 | +11 | 1d16 | 6 | ARROW2, NO_FEAR |
| 8 | Balcmeg | Orc | 15d4 | 2 | +4 | +8 | 3d8 | 9 | ⚠️ Lower evasion than regulars |
| 8 | Lug | Orc | 10d4 | 2 | +16 | +16 | 1d7 | 5 | CRUEL_BLOW, glass cannon |
| 9 | Orcobal | Orc | 15d4 | 2 | +8 | +10 | 3d10 | 8 | RALLY (POW_14) |
| 10 | Othrod | Orc | 15d4 | 2 | +9 | +15 | 2d9 | 9 | Dual-wield, ESCORTS |
| 11 | Uldor | Easterling | 14d4 | 2 | +13 | +16 | 2d7 | 11 | ARROW1 @ 75% cast, CRIPPLING |
| 11 | Brodda | Undead | 8d4 | 2 | +8 | +15 | 2d9 | 12 | Quest spawn, CON drain |
| 12 | Duruin | Balrog | 28d4 | 2 | +13 | +18 | 2d12 | 12 | Fire damage, whip disarm |
| 12 | Ulfang | Easterling | 15d4 | 2 | +13 | +19 | 2d8 | 11 | RALLY, OPPORTUNIST |
| 13 | Gilim | Giant | 35d4 | 2 | +9 | +10 | 4d9 | 10 | BOULDER spell, low armor |
| 14 | Delthaur | Balrog | 32d4 | 2 | +15 | +20 | 3d10 | 14 | SCARE spell |
| 14 | Nan | Giant | 40d4 | 2 | +11 | +15 | 3d13 | 11 | Huge HP pool |
| 20 | Draugluin | Werewolf | 22d4 | 3 | +24 | +26 | 3d9 | 15 | Speed 3!, ESCORT |
| 20 | Vallach | Balrog | 44d4 | 3 | +21 | +26 | 3d12 | 19 | Speed 3!, massive HP |
| 21 | Dagorhir | Troll | 24d4 | 2 | +17 | +17 | 4d10 | 10 | ⚠️ ELFBANE, KNOCK_BACK |
| 21 | Gostir | Dragon | 60d4 | 2 | +14 | +24 | 2d23 | 22 | ⚠️ Mental dragon, low armor |
| 23 | Ungoliant | Spider | 50d4 | 2 | +25 | +31 | 3d21 | 23 | BRTH_DARK, god-tier |
| 24 | Glaurung | Dragon | 60d4 | 2 | +16 | +29 | 2d25 | 24 | Father of dragons |
| 24 | Gorthaur | Wolf-Maia | 30d4 | 3 | +29 | +31 | 3d11 | 24 | Speed 3, evasion 29 |
| 25 | Carcharoth | Wolf | 30d4 | 4 | +30 | +29 | 2d17 | 20 | **Speed 4!**, fastest in game |
| 25 | Morgoth | Vala | 160d4 | 2 | +20→30 | +20→40 | 6d10→8d10 | 25→40 | Multi-phase boss |

⚠️ = Potentially underpowered for depth

---

## Power Curve Analysis

### Hit Points by Depth
```
Depth  5: 12d4  (30 avg)
Depth  8: 15d4  (37 avg)
Depth 10: 15d4  (37 avg)
Depth 12: 28d4  (70 avg) - Balrog jump
Depth 13: 35d4  (87 avg) - Giant jump
Depth 14: 40d4  (100 avg)
Depth 20: 44d4  (110 avg) - Balrog
Depth 21: 60d4  (150 avg) - Dragon
Depth 23: 50d4  (125 avg)
Depth 24: 60d4  (150 avg)
Depth 25: 160d4 (400 avg) - Morgoth
```

**Observation**: Major HP jumps occur at:
- Depth 12 (Balrogs appear): +86% HP
- Depth 13 (Giants appear): +24% HP
- Depth 21 (Ancient dragons): +36% HP
- Depth 25 (Morgoth): +166% HP

The curve is **exponential** rather than linear, which is appropriate for an escalating dungeon.

---

### Evasion by Depth
```
Depth  5: +4
Depth  8: +4 to +16 (Balcmeg vs Lug - wide variance)
Depth 10: +9
Depth 12: +13
Depth 14: +15
Depth 20: +21 to +24
Depth 24: +16 (Glaurung) to +29 (Gorthaur)
Depth 25: +30 (Carcharoth, max possible)
```

**Observation**: Evasion scales more linearly than HP, from +4 to +30 over 20 depths. Dragons tend to have lower evasion (tank design), while speed-based enemies (werewolves, wolf-form Gorthaur) have extremely high evasion.

---

### Speed Distribution
- **Speed 2 (Normal)**: Most uniques (75%)
- **Speed 3 (Fast)**: Draugluin, Vallach, Gorthaur (depth 20+)
- **Speed 4 (Blinding)**: Carcharoth ONLY

**Observation**: Speed is the **scarcest** stat increase. Only 4 uniques have speed >2, and all are depth 20+. This makes speed extremely valuable - a speed 3 enemy takes 50% more actions than speed 2.

---

### Will (Mental Resistance) by Depth
```
Depth  5: 5
Depth  8: 9
Depth 10: 9
Depth 12: 12
Depth 14: 14
Depth 20: 19
Depth 24: 24 (Glaurung, Gorthaur - tied highest)
Depth 25: 25→40 (Morgoth scaling)
```

**Observation**: Will scales consistently, roughly +1 per 2 depths. The jump to Will 24 at depth 24 represents "god-tier" mental defenses (Glaurung the Deceiver, Gorthaur the Maia). Morgoth's scaling to Will 40 is 66% higher than anything else.

---

## Unique Distribution by Creature Type

### Orcs (Most Common)
- Depth 5: Gorgol
- Depth 7: Boldog
- Depth 8: Balcmeg, Lug
- Depth 9: Orcobal
- Depth 10: Othrod

**Total: 6 unique orcs** spanning depths 5-10
**Analysis**: Orcs are the "early game threat" with consistent progression from basic captain (Gorgol) to overlord (Othrod). Good variety in combat styles (tank, glass cannon, leader).

---

### Easterlings (Human Traitors)
- Depth 11: Uldor, Brodda (undead)
- Depth 12: Ulfang

**Total: 3 unique Easterlings** at depth 11-12
**Analysis**: Easterlings represent the "mid-game human threat" with emphasis on ranged attacks and treachery. The undead Brodda is thematically appropriate (a traitor cursed after death).

---

### Giants
- Depth 13: Gilim
- Depth 14: Nan

**Total: 2 unique giants** at depth 13-14
**Analysis**: Giants are the "tank" archetype - massive HP, low armor, big single-hit damage. Both have BOULDER spells for ranged capability.

---

### Balrogs (Raukar)
- Depth 12: Duruin (Least)
- Depth 14: Delthaur (Terror)
- Depth 20: Vallach (Sudden Flame)

**Total: 3 named Balrogs** at depths 12, 14, 20
**Analysis**: Balrogs show excellent power scaling:
- Duruin: 28 HP, speed 2, evasion +13 - "Least" but still terrifying
- Delthaur: 32 HP, speed 2, evasion +15, SCARE - "Terror" specialist
- Vallach: 44 HP, speed 3, evasion +21 - "Sudden Flame" justified by speed

The naming ("Least" → "Terror" → "Sudden Flame") reflects mechanical progression.

---

### Wolves/Werewolves
- Depth 20: Draugluin (Werewolf Sire)
- Depth 24: Gorthaur (Wolf-form Maia)
- Depth 25: Carcharoth (Guardian Wolf)

**Total: 3 unique wolves** at depths 20+
**Analysis**: All late-game, all speed 3-4. This creates a "wolf rush" threat in the endgame. Carcharoth's speed 4 is unique and thematically perfect for "the first guard of Angband."

---

### Dragons
- Depth 21: Gostir (mental dragon)
- Depth 24: Glaurung (Father of Dragons)

**Total: 2 named dragons**
**Analysis**: Surprisingly few unique dragons. Gostir is the mental/fear specialist (no fire breath). Glaurung is the classic fire-breather with hypnotic Will 24. Both have 60d4 HP (massive).

---

### Spiders
- Depth 23: Ungoliant

**Total: 1 unique spider**
**Analysis**: Ungoliant is THE spider - no other named spiders exist. This makes sense as she's the mother of all spiders in Tolkien lore. The lack of lesser unique spiders (e.g., Shelob equivalent) could be an opportunity for expansion.

---

### Trolls
- Depth 21: Dagorhir (Elfbane)

**Total: 1 unique troll**
**Analysis**: Only one named troll feels light, especially compared to 6 named orcs. Trolls are common enemies throughout the game but lack unique representation beyond Dagorhir.

---

## Depth Gaps in Unique Coverage

| Depth Range | Unique Count | Notes |
|-------------|--------------|-------|
| 1-4 | 0 | No combat uniques |
| 5-10 | 6 | Dense orc progression |
| 11-12 | 4 | Easterling cluster |
| 13-14 | 4 | Giants + Balrog |
| 15-19 | **0** | ⚠️ **LARGE GAP** |
| 20-21 | 4 | Speed demons + dragons |
| 22 | 0 | Small gap |
| 23-25 | 5 | Endgame cluster |

**Major Finding**: **No unique monsters at depths 15-19** (5-depth gap)

### Recommendations for Gap Filling
1. **Depth 16**: Unique cave troll chieftain (REGENERATE, KNOCK_BACK, high HP)
2. **Depth 17**: Unique vampire lord with DRAIN_EXP
3. **Depth 18**: Unique fire-drake (adult dragon, pre-Gostir)
4. **Depth 19**: Unique shadow-horror (PASS_WALL, mental attacks)

This would smooth the difficulty curve and provide more memorable encounters in the mid-late game.

---

## Spell Power Distribution

| Unique | Depth | Spell | Power | Cast % | Analysis |
|--------|-------|-------|-------|--------|----------|
| Gorgol | 5 | RALLY | 12 | 5% | Leadership starter |
| Boldog | 7 | ARROW2 | 9 | 15% | Early ranged threat |
| Orcobal | 9 | RALLY | 14 | 5% | Improved leader |
| Othrod | 10 | RALLY | 16 | 5% | Best orc leader |
| Uldor | 11 | ARROW1 | **21** | **75%** | ⚠️ Extremely dangerous |
| Brodda | 11 | SLOW, HOLD | - | 20% | Debuff specialist |
| Ulfang | 12 | RALLY | 18 | 5% | Peak human leader |
| Gilim | 13 | BOULDER | 10 | 20% | Giant ranged |
| Delthaur | 14 | SCARE | - | 15% | Fear specialist |
| Nan | 14 | BOULDER | 12 | 10% | Giant ranged |
| Gostir | 21 | CONF/SCARE/HOLD | - | 20% | Mental dragon |
| Ungoliant | 23 | BRTH_DARK | **34** | 25% | God-tier power |
| Glaurung | 24 | BRTH_FIRE/SCARE/HOLD/CONF | **28** | **40%** | High frequency |
| Gorthaur | 24 | HOLD/SNG_OATHS | - | 25% | Control specialist |
| Morgoth | 25 | EARTHQUAKE/SNG_BINDING/SNG_PIERCING | - | 25% | Boss spells |

**Observation**: Spell power and cast frequency are independent threat multipliers:
- **Uldor** (POW_21 @ 75%) is more dangerous than **Ungoliant** (POW_34 @ 25%) in terms of spell spam
- **Glaurung** (POW_28 @ 40%) casts spells **8x more often** than Othrod (POW_16 @ 5%)

---

## Special Ability Distribution

### ESCORT/ESCORTS (Summons Allies)
- Gorgol, Orcobal, Othrod, Uldor, Ulfang, Draugluin, Dagorhir, Gorthaur
- **8 uniques** (30% of combat uniques)
- Clustered in depths 5-12 and 20+

### DROP_GREAT (Best Loot)
- Duruin, Delthaur, Vallach, Dagorhir, Gostir, Ungoliant, Glaurung, Gorthaur, Morgoth
- **9 uniques** (35% of combat uniques)
- Almost all depth 12+

### CRUEL_BLOW (Critical Hit Specialist)
- Lug only
- Unique mechanic, could be expanded

### ELFBANE (Anti-Elf)
- Dagorhir only
- Unique mechanic, could be expanded

### CRIPPLING (Stat Damage)
- Uldor only
- Unique mechanic, could be expanded

### KNOCK_BACK (Forced Movement)
- Dagorhir only (among uniques)
- Common on trolls generally

### SPEED 3+
- Draugluin, Vallach, Gorthaur, Carcharoth
- All depth 20+
- **Most dangerous trait** in late game

---

## Conclusion

### Strengths of Current Unique Design
1. ✅ Excellent power scaling from depth 5 to 25
2. ✅ Diverse combat archetypes (tanks, glass cannons, speedsters, leaders)
3. ✅ Thematic consistency (Balrogs are fire/shadow, werewolves are fast/evasive, giants are high HP/low armor)
4. ✅ Spell variety creates different tactical challenges
5. ✅ Orc progression (6 uniques) provides consistent early-game milestones

### Opportunities for Improvement
1. ⚠️ **Depth 15-19 gap**: 5 consecutive depths with no uniques
2. ⚠️ **Balcmeg** needs evasion buff (+2-3 to match/exceed Orc Champion)
3. ⚠️ **Dagorhir** feels weak for depth 21 (consider +5d4 HP or +1d4 protection)
4. ⚠️ **Gostir** has lower defenses than depth 18 regular dragons (intentional glass cannon, but could use +2 evasion)
5. ⚠️ Limited unique trolls (1) and spiders (1) compared to orcs (6)

### Suggested New Uniques
- **Depth 16**: "Gothmog, Troll Chieftain" - 28d4 HP, REGENERATE, ESCORT, KNOCK_BACK
- **Depth 17**: "Thuringwethil, the Vampire Queen" - Speed 3, FLYING, DRAIN_EXP
- **Depth 18**: "Ancalagon the Black (Young)" - 45d4 HP, BRTH_FIRE, Speed 2
- **Depth 19**: "Nameless Horror" - PASS_WALL, INVISIBLE, CONF/SCARE

This would create a smoother difficulty curve and more memorable late-mid-game encounters.
