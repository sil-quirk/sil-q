# Unique Monster Power Analysis - Sil-Q

## Executive Summary

This analysis compares unique monsters with general (non-unique) monsters of the same type and similar or higher depth levels. The goal is to determine whether uniques are appropriately powerful compared to their non-unique counterparts.

## Methodology

Compared unique monsters against:
1. General monsters of the same creature type at similar depths (±2 levels)
2. General monsters at the exact same depth level
3. General monsters at higher depths

Key stats analyzed:
- **Health (HP)**: Hit dice (e.g., 12d4)
- **Speed**: Movement/action speed
- **Combat**: Evasion, Protection, Attack bonus, Damage dice
- **Mental**: Perception, Will, Stealth
- **Special abilities**: Spells, flags (SMART, CRUEL_BLOW, etc.)

---

## Early Game Uniques (Depth 5-10)

### Gorgol, the Butcher (Depth 5)
**Unique Orc Captain**
- HP: 12d4, Speed: 2, Evasion: +4, Protection: 3d4
- Attack: +6, 3d7 damage
- Perception: 2, Will: 5, Stealth: 1
- Spells: RALLY (POW_12, 5% cast chance)
- Special: ESCORT, DROP_100, DROP_GOOD

**Comparison to Orc Skirmisher (Depth 2)**
- HP: 7d4, Speed: 2, Evasion: +3, Protection: 1d4
- Attack: +2, 2d6 damage
- Perception: 1, Will: 1, Stealth: 1
- No spells
- **Power Ratio**: Gorgol has ~70% more HP, +33% evasion, +300% protection, +200% attack bonus, +40% damage

**Analysis**: Appropriately powerful for depth 5. Gorgol represents a significant step up from basic orcs, with leadership abilities (RALLY) and guaranteed good drops.

---

### Boldog, the Merciless (Depth 7)
**Unique Orc Captain**
- HP: 13d4, Speed: 2, Evasion: +6, Protection: 3d4
- Attack: +11, 1d16 damage
- Perception: 4, Will: 6, Stealth: 1
- Spells: ARROW2 (POW_9, 15% cast chance)
- Special: ESCORTS, NO_FEAR

**Comparison to Orc Warrior (Depth 6)**
- HP: 9d4, Speed: 2, Evasion: +5, Protection: 2d4
- Attack: +3, 3d6 damage
- Perception: 2, Will: 4, Stealth: 1
- No spells
- **Power Ratio**: Boldog has +44% HP, +20% evasion, +50% protection, +266% attack bonus

**Comparison to Tattered Wight (Depth 7)**
- HP: 4d4, Speed: 1, Evasion: +5, Protection: 3d4
- Attack: +9, 2d9 damage
- Has spells: DARKNESS, SLOW
- **Power Ratio**: Boldog has 325% more HP, +200% speed

**Analysis**: Boldog is significantly more dangerous than depth 6-7 regulars. The ranged attack (ARROW2) and escorts make this a challenging encounter. However, the 1d16 damage is swingy (1-16 range).

---

### Balcmeg, the Relentless (Depth 8)
**Unique Orc Champion**
- HP: 15d4, Speed: 2, Evasion: +4, Protection: 4d4
- Attack: +8, 3d8 damage
- Perception: 3, Will: 9, Stealth: 1
- No spells

**Comparison to Orc Champion (Depth 8)**
- HP: 11d4, Speed: 2, Evasion: +5, Protection: 3d4
- Attack: +6, 3d8 damage
- Perception: 3, Will: 5, Stealth: 1
- **Power Ratio**: Balcmeg has +36% HP, -20% evasion (!), +33% protection, +33% attack bonus, same damage

**Analysis**: **CONCERN** - Balcmeg actually has LOWER evasion than the general Orc Champion at the same depth. The main advantages are +4 HP dice and +2 attack bonus. For a unique, this feels underwhelming. The DROP_100 and DROP_GOOD help compensate from a reward perspective, but combat-wise this is barely stronger than a regular champion.

---

### Lug, the Grotesque (Depth 8)
**Unique Orc Assassin**
- HP: 10d4, Speed: 2, Evasion: +16, Protection: 2d4
- Attack: +16, 1d7 damage
- Perception: 5, Will: 5, Stealth: 1
- Special: CRUEL_BLOW, DROP_GREAT

**Comparison to Orc Champion (Depth 8)**
- HP: 11d4, Speed: 2, Evasion: +5, Protection: 3d4
- Attack: +6, 3d8 damage
- **Power Ratio**: Lug has -9% HP, +220% evasion (!), -33% protection, +166% attack bonus, -70% damage dice

**Analysis**: Lug is a **glass cannon** design - incredibly high evasion and attack bonus but low HP and damage dice. The CRUEL_BLOW flag (critical hit specialist) compensates for the low 1d7 base damage. This is an interesting unique with a distinct combat style - hard to hit, high precision striker. Well-differentiated from Balcmeg.

---

### Orcobal, Champion of the Orcs (Depth 9)
**Unique Orc Supreme Champion**
- HP: 15d4, Speed: 2, Evasion: +8, Protection: 3d4
- Attack: +10, 3d10 damage
- Perception: 5, Will: 8, Stealth: 1
- Spells: RALLY (POW_14, 5% cast chance)
- Special: ESCORT

**Comparison to Orc Captain (Depth 9)**
- HP: 10d4, Speed: 2, Evasion: +7, Protection: 3d4
- Attack: +10, 2d8 damage
- Spells: RALLY (POW_10, 5% cast chance)
- **Power Ratio**: Orcobal has +50% HP, +14% evasion, same attack bonus, +60% damage

**Analysis**: Good progression - Orcobal is clearly the pinnacle of orc leadership. The +POW_4 spell power and +50% HP make a meaningful difference. Well-balanced unique for depth 9.

---

### Othrod, the Orc Lord (Depth 10)
**Unique Orc Overlord**
- HP: 15d4, Speed: 2, Evasion: +9, Protection: 4d4
- Attack: +15, 2d9 damage (main), +12, 2d4 damage (whip/disarm)
- Perception: 6, Will: 9, Stealth: 1
- Spells: RALLY (POW_16, 5% cast chance)
- Special: ESCORTS, DROP_GREAT

**Comparison to Orc Captain (Depth 9)**
- HP: 10d4, Evasion: +7, Protection: 3d4
- Attack: +10, 2d8 damage
- **Power Ratio**: Othrod has +50% HP, +28% evasion, +33% protection, +50% attack bonus

**Comparison to Distended Spider (Depth 10)**
- HP: 20d4, Evasion: +7, Protection: 0
- Attack: +9, 2d11 damage
- **Power Ratio**: Othrod has -25% HP but +28% evasion, massive protection advantage, +66% attack bonus

**Analysis**: Strong unique. The dual-wielding (main attack + whip disarm) adds tactical depth. The ESCORTS flag and high Will/Perception make this a dangerous encounter. Well-powered for depth 10.

---

## Mid Game Uniques (Depth 11-15)

### Uldor, the Accursed (Depth 11)
**Unique Easterling Traitor**
- HP: 14d4, Speed: 2, Evasion: +13, Protection: 3d4, Light: 1
- Attack: +16, 2d7 damage
- Perception: 6, Will: 11, Stealth: 1
- Spells: ARROW1 (POW_21, 75% cast chance)
- Special: MAN, ESCORT, DROP_100, DROP_GOOD, CRIPPLING, TAKE_ITEM

**Comparison to Easterling Warrior (Depth 8)**
- HP: 10d4, Evasion: +5, Protection: 3d4
- Attack: +7, 2d8 damage
- Will: 7
- No spells

**Comparison to Easterling Archer (Depth 10)**
- HP: 9d4, Evasion: +9, Protection: 2d4
- Attack: +9, 1d7 damage
- Spells: ARROW2 (POW_11, 75% cast chance)

**Analysis**: Uldor is extremely dangerous. The 75% spell cast rate with POW_21 arrows makes him a ranged threat comparable to depth 15+ monsters. The CRIPPLING flag (stat damage) and high Will make this a very tough unique. Well-balanced as a "boss traitor" archetype.

---

### Brodda, the Easterling Lord (Depth 11)
**Unique Undead Easterling (Wight)**
- HP: 8d4, Speed: 2, Evasion: +8, Protection: 3d4
- Attack: +15 (touch/LOSE_CON), +15, 2d9 damage
- Perception: 12, Will: 12, Stealth: 5
- Spells: SLOW, HOLD (20% cast chance)
- Special: UNDEAD, DROP_100, DROP_GREAT, SPECIAL_GEN (quest monster)

**Comparison to Tattered Wight (Depth 7)**
- HP: 4d4, Evasion: +5, Protection: 3d4
- Attack: +9, 2d9 damage; +9 touch/LOSE_DEX
- Perception: 6, Will: 5, Stealth: 5
- Spells: DARKNESS, SLOW

**Analysis**: Brodda has **double** the HP of a regular wight, +60% evasion, double the Perception, and far superior Will (12 vs 5). The CON drain is more dangerous than DEX drain. However, as a quest-specific spawn (SPECIAL_GEN), the power level is appropriate. Regular depth 11 encounters wouldn't see this.

---

### Duruin, Least of the Balrogs (Depth 12)
**Unique Balrog (Rauko)**
- HP: 28d4, Speed: 2, Evasion: +13, Protection: 4d4, Light: -3
- Attack: +18, 2d12 (FIRE); +14, 3d6 (WHIP/DISARM)
- Perception: 7, Will: 12, Stealth: 1
- Special: RAUKO, NO_FEAR, RES_FIRE, HURT_COLD, DROP_100, DROP_GREAT

**Comparison to Sulrauko (Wind Spirit, Depth 12)**
- HP: 5d4, Evasion: +14, Protection: 2d4
- Attack: +14, 3d5 damage
- Perception: 7, Will: 10
- Special: RAUKO, INVISIBLE, FLYING

**Comparison to Fire-drake Hatchling (Depth 12)**
- HP: 10d4, Evasion: +11, Protection: 1d4
- Attack: +13, 2d5 (FIRE bite)
- Spells: BRTH_FIRE (POW_6)

**Analysis**: Duruin has **560%** more HP than Sulrauko and **280%** more than the fire-drake. This is a massive power spike. The dual attacks (fire hit + disarm whip) and high protection make this extremely dangerous. As the "Least" Balrog, this sets a baseline for balrog power that is appropriately terrifying for depth 12.

---

### Ulfang the Black (Depth 12)
**Unique Easterling Chieftain**
- HP: 15d4, Speed: 2, Evasion: +13, Protection: 4d4, Light: 1
- Attack: +19, 2d8 damage
- Perception: 6, Will: 11, Stealth: 1
- Spells: RALLY (POW_18, 5% cast chance)
- Special: MAN, ESCORT, DROP_2D2, DROP_GOOD, OPPORTUNIST

**Comparison to Easterling Spy (Depth 12)**
- HP: 8d4, Evasion: +13, Protection: 1d4
- Attack: +12, 1d8 damage
- Perception: 12, Will: 8, Stealth: 15
- Spells: ARROW1, SHRIEK (POW_15, 20% cast chance)

**Analysis**: Ulfang trades the spy's stealth and ranged attacks for superior HP (+87%), protection (+300%), and melee power (+58% attack, +100% damage). The OPPORTUNIST flag (flanking/backstab specialist) adds tactical complexity. Good unique design - clearly more powerful but with different combat role than the spy.

---

### Gilim, the Giant of Eruman (Depth 13)
**Unique Giant**
- HP: 35d4, Speed: 2, Evasion: +9, Protection: 1d4, Light: 2
- Attack: +10, 4d9 (COLD) damage
- Perception: 5, Will: 10, Stealth: 0
- Spells: BOULDER (POW_10, 20% cast chance)
- Special: DROP_GOOD, RES_COLD

**Comparison to Mountain Troll (Depth 7)**
- HP: 14d4, Evasion: +3, Protection: 2d4
- Attack: +6, 4d5 damage
- Perception: 1, Will: 2

**Comparison to Snow Troll (Depth 11)**
- HP: 16d4, Evasion: +7, Protection: 2d4
- Attack: +9, 4d6 damage; +8, 2d9 (COLD bite)
- Will: 3

**Analysis**: Gilim has **218%** of the snow troll's HP and massively superior Will (10 vs 3). The BOULDER spell (ranged attack) is unique to giants. However, the protection is notably LOW (1d4 vs trolls' 2d4), making him a "big HP pool but hittable" enemy. The damage dice (4d9 = 18 avg) is excellent. Well-balanced glass cannon design for a giant.

---

### Delthaur, Balrog of Terror (Depth 14)
**Unique Balrog**
- HP: 32d4, Speed: 2, Evasion: +15, Protection: 4d4, Light: -3
- Attack: +20, 3d10 (FIRE); +16, 3d8 (WHIP/DISARM)
- Perception: 7, Will: 14, Stealth: 1
- Spells: SCARE (15% cast chance)
- Special: RAUKO, NO_FEAR, RES_FIRE, HURT_COLD, DROP_100, DROP_GREAT

**Comparison to Duruin (Depth 12)**
- HP: 28d4, Evasion: +13, Protection: 4d4
- Attack: +18, 2d12; +14, 3d6

**Analysis**: Delthaur is +14% HP, +15% evasion, +11% attack bonus, and adds the SCARE spell over Duruin. This is appropriate power scaling for a depth 14 Balrog vs depth 12. The "Balrog of Terror" name is mechanically supported by the SCARE spell.

---

### Nan, the Giant (Depth 14)
**Unique Giant**
- HP: 40d4, Speed: 2, Evasion: +11, Protection: 1d4, Light: 2
- Attack: +15, 3d13 damage
- Perception: 5, Will: 11, Stealth: 0
- Spells: BOULDER (POW_12, 10% cast chance)
- Special: DROP_CHOSEN, DROP_GOOD

**Comparison to Gilim (Depth 13)**
- HP: 35d4, Evasion: +9, Protection: 1d4
- Attack: +10, 4d9 damage
- Will: 10

**Comparison to Cave Troll (Depth 15)**
- HP: 18d4, Evasion: +11, Protection: 2d4
- Attack: +13, 4d7; +12, 2d11 (bite)
- Will: 4

**Analysis**: Nan has **222%** the HP of a depth 15 cave troll but half the protection. The damage consolidation into a single 3d13 attack (32.5 avg) vs cave troll's split 4d7+2d11 (26 avg) is a net gain. Giants maintain the pattern of "huge HP, low armor" consistently. Well-designed.

---

## Late Game Uniques (Depth 20-25)

### Draugluin, Sire of Werewolves (Depth 20)
**Unique Werewolf Lord**
- HP: 22d4, Speed: 3, Evasion: +24, Protection: 3d4
- Attack: +26, 3d9 (WOUND claw); +23, 2d13 (POISON bite)
- Perception: 17, Will: 15, Stealth: 5
- Special: ESCORT, DROP_CHOSEN, WOLF

**Comparison to Werewolf (Depth 13)**
- HP: 12d4, Speed: 3, Evasion: +14, Protection: 2d4
- Attack: +16, 3d5 (WOUND claw); +13, 2d9 (POISON bite)
- Perception: 9, Will: 9, Stealth: 7

**Analysis**: Draugluin has +83% HP, +71% evasion, +50% protection, +62% attack bonus, +80% claw damage, +44% bite damage, +88% Perception, +66% Will. This is a **massive** power increase befitting "Sire of Werewolves." The speed 3 makes this incredibly dangerous. Excellent scaling.

---

### Vallach, Balrog of Sudden Flame (Depth 20)
**Unique Balrog**
- HP: 44d4, Speed: 3 (!), Evasion: +21, Protection: 5d4, Light: -3
- Attack: +26, 3d12 (FIRE); +22, 3d8 (WHIP/DISARM)
- Perception: 12, Will: 19, Stealth: 1
- Special: RAUKO, NO_FEAR, RES_FIRE, HURT_COLD, DROP_100, DROP_GREAT

**Comparison to Delthaur (Depth 14)**
- HP: 32d4, Speed: 2, Evasion: +15, Protection: 4d4
- Attack: +20, 3d10; +16, 3d8

**Comparison to Gwathrauko (Depth 20 regular Rauko)**
- HP: 8d4, Speed: 2, Evasion: +18, Protection: 3d4
- Attack: +18, 2d8 (DARK)

**Analysis**: Vallach has **550%** the HP of the regular depth 20 Rauko and **+50% speed** (3 vs 2) which is HUGE. The +37% HP and +40% evasion over Delthaur, plus the speed increase, makes this an appropriately terrifying depth 20 unique. The name "Sudden Flame" is justified by speed 3.

---

### Dagorhir, the Elfbane (Depth 21)
**Unique Troll Lord**
- HP: 24d4, Speed: 2, Evasion: +17, Protection: 4d4
- Attack: +17, 4d10 damage
- Perception: 6, Will: 10, Stealth: 0
- Special: ESCORT, ELFBANE, KNOCK_BACK, REGENERATE, NO_FEAR, TROLL

**Comparison to Cave Troll (Depth 15)**
- HP: 18d4, Evasion: +11, Protection: 2d4
- Attack: +13, 4d7; +12, 2d11 bite
- Will: 4

**Analysis**: +33% HP, +54% evasion, +100% protection, +30% attack bonus. The ELFBANE flag (bonus vs elves) and ESCORT make this a challenging fight. However, for depth 21, this feels slightly underpowered compared to other depth 20-21 uniques. Cave trolls at depth 15 aren't far behind.

---

### Gostir, the Dread Glance (Depth 21)
**Unique Dragon**
- HP: 60d4, Speed: 2, Evasion: +14, Protection: 2d4
- Attack: +24, 3d13 (claw); +19, 2d23 (WOUND bite)
- Perception: 13, Will: 22, Stealth: 0
- Spells: CONF, SCARE, HOLD (20% cast chance)
- Special: DRAGON, TERRITORIAL, DROP_GREAT

**Comparison to Fire-drake (Depth 18)**
- HP: 35d4, Evasion: +16, Protection: 3d4
- Attack: +20, 2d13 (FIRE claw); +16, 2d15 (FIRE bite)
- Perception: 9, Will: 17
- Spells: BRTH_FIRE, CONF, SCARE (POW_18, 20% cast chance)

**Analysis**: Gostir has +71% HP, -12% evasion (!), -33% protection (!), but massively superior Will (+29%) and the deadly 2d23 bite (47 avg damage). The "Dread Glance" is represented by CONF/SCARE/HOLD spells. This is a **mental/fear dragon** rather than a fire dragon, with lower defenses but massive HP and psychological attacks. Interesting unique design.

---

### Ungoliant, the Unlight (Depth 23)
**Unique Ancient Spider**
- HP: 50d4, Speed: 2, Evasion: +25, Protection: 3d4
- Attack: +31, 3d21 (POISON bite)
- Perception: 15, Will: 23, Stealth: 3
- Spells: BRTH_DARK (POW_34, 25% cast chance)
- Special: SPIDER, DROP_GREAT, RES_POIS, ATTR_MULTI, NO_FEAR

**Comparison to Ancient Spider (Depth 21)**
- HP: 20d4, Evasion: +19, Protection: 1d4
- Attack: +22, 2d19 (POISON bite)
- Perception: 8, Will: 16

**Analysis**: Ungoliant has **250%** the HP, +31% evasion, +200% protection, +40% attack bonus, +75% damage, +87% Perception, +43% Will. Plus the unique BRTH_DARK ability. This is appropriately god-tier for the mother of all spiders. The Unlight breath is thematically perfect.

---

### Glaurung, the Deceiver (Depth 24)
**Unique Dragon Father**
- HP: 60d4, Speed: 2, Evasion: +16, Protection: 2d4
- Attack: +29, 3d15 (claw); +24, 2d25 (FIRE bite)
- Perception: 10, Will: 24, Stealth: 0
- Spells: BRTH_FIRE, SCARE, HOLD, CONF (POW_28, 40% cast chance)
- Special: DRAGON, DROP_GREAT, RES_FIRE, HURT_COLD, TERRITORIAL

**Comparison to Cold-drake (Depth 22)**
- HP: 50d4, Evasion: +18, Protection: 3d4
- Attack: +26, 3d15; +22, 2d21
- Spells: BRTH_COLD, SCARE, CONF (POW_25, 30% cast chance)

**Analysis**: Glaurung has +20% HP, -11% evasion, -33% protection, but +19% bite damage and +33% spell casting frequency. The "Father of Dragons" is only marginally more powerful than depth 22 cold-drakes, which feels appropriate - he's OLD and powerful but not overwhelmingly so. The high Will (24) represents his legendary deception/hypnosis.

---

### Gorthaur/Sauron (Depth 24)
**Unique Wolf-form Maia**
- HP: 30d4, Speed: 3, Evasion: +29, Protection: 3d4
- Attack: +31, 3d11 (WOUND claw); +28, 2d15 (POISON bite)
- Perception: 20, Will: 24, Stealth: 5
- Spells: HOLD, SNG_OATHS (25% cast chance)
- Special: WOLF, ESCORT, DROP_GREAT, SMART

**Comparison to Draugluin (Depth 20)**
- HP: 22d4, Speed: 3, Evasion: +24, Protection: 3d4
- Attack: +26, 3d9; +23, 2d13
- Perception: 17, Will: 15

**Analysis**: Gorthaur has +36% HP, +20% evasion, +19%/+19% attack bonus, +22%/+15% damage, +17% Perception, **+60% Will**. The Will 24 is tied with Glaurung - appropriately god-tier. However, the HP (30d4) feels LOW for Morgoth's greatest servant. This is likely intentional - he's dangerous due to speed 3, evasion 29, and mental powers, not raw HP.

---

### Carcharoth, the Jaws of Thirst (Depth 25)
**Unique Guardian Wolf**
- HP: 30d4, Speed: 4 (!), Evasion: +30, Protection: 2d4
- Attack: +29, 2d17 (POISON bite)
- Perception: 20, Will: 20, Stealth: 5
- Special: WOLF, SMART, FORCE_DEPTH (surface guardian)

**Comparison to Gorthaur (Depth 24)**
- HP: 30d4, Speed: 3, Evasion: +29, Protection: 3d4
- Attack: +31, 3d11; +28, 2d15
- Perception: 20, Will: 24

**Analysis**: Carcharoth has identical HP, **+33% speed** (4 vs 3 - fastest in the game!), +3% evasion (30 is max), -33% protection, and a single devastating bite (35 avg damage). The speed 4 makes this the fastest enemy in the game. As the "final surface guardian," this is appropriately terrifying. The trade-off is lower protection and Will than Gorthaur.

---

### Morgoth, Lord of Darkness (Depth 25)
**Unique Final Boss**
- HP: 160d4 (!), Speed: 2, Evasion: +20, Protection: 5d4, Light: 7
- Attack: +20, 6d10 (SHATTER damage)
- Perception: 10, Will: 25, Stealth: 0
- Spells: EARTHQUAKE, SNG_BINDING, SNG_PIERCING (25% cast chance)
- Special: QUESTOR, NO_FEAR, TUNNEL_WALL, DROP_GREAT, DROP_CHOSEN

**Note**: Stats change when angered:
- Decrowned: Perception → 15
- Hurt: Evasion → 25, Attack → +30, Damage → 7d10, Will → 30, Perception → 20
- Badly Hurt: Protection → 7d4, Will → 35, Perception → 25
- Desperate: Evasion → 30, Attack → +40, Damage → 8d10, Will → 40, Perception → 30

**Comparison to ALL other monsters**
- HP: **533%** more than Glaurung (60d4)
- Speed: Lower than Carcharoth/Gorthaur/Draugluin
- Evasion: Starts at +20, scales to +30 (max in game when desperate)
- Will: Starts at 25 (tied highest), scales to 40 (far beyond anything else)

**Analysis**: Morgoth is designed as a **multi-phase boss** with escalating power. The initial stats are actually moderate for depth 25, but as he's wounded, he becomes progressively more dangerous. The HP pool (160d4 = 400 avg) is astronomical - designed for a long boss fight. The TUNNEL_WALL ability (can burrow through walls) adds unique mechanics. Well-designed final boss with appropriate god-tier scaling.

---

## Overall Findings

### Well-Balanced Uniques
- **Gorgol** (Depth 5): Good introduction to unique orcs with leadership
- **Orcobal** (Depth 9): Clear pinnacle of orc champions
- **Uldor** (Depth 11): Dangerous ranged unique with high spell power
- **Duruin** (Depth 12): Appropriately terrifying "least" Balrog
- **Draugluin** (Depth 20): Massive power increase befitting "Sire"
- **Vallach** (Depth 20): Speed 3 Balrog is appropriately deadly
- **Ungoliant** (Depth 23): God-tier spider with perfect scaling
- **Gorthaur** (Depth 24): Glass cannon with extreme evasion/speed/will
- **Carcharoth** (Depth 25): Speed 4 makes this terrifying
- **Morgoth** (Depth 25): Multi-phase boss design is excellent

### Potentially Underpowered Uniques
- **Balcmeg** (Depth 8): Lower evasion than non-unique Orc Champion at same depth. Main advantages are +4 HP dice and +2 attack. Feels weak for a unique.
- **Dagorhir** (Depth 21): Only +33% HP over depth 15 cave trolls. For depth 21, could use more differentiation.
- **Gostir** (Depth 21): Lower evasion and protection than depth 18 fire-drake. The mental focus is interesting but defensive stats feel low.

### Unique Design Patterns
1. **Glass Cannons**: Lug, Gorthaur - high evasion/attack, low HP/protection
2. **Tanks**: Giants (Gilim, Nan) - massive HP, low protection
3. **Speed Demons**: Vallach, Carcharoth, Gorthaur - speed 3-4 with high evasion
4. **Leadership**: Gorgol, Orcobal, Othrod, Ulfang - RALLY spells + ESCORT
5. **Mental/Fear**: Gostir, Delthaur, Glaurung - SCARE/CONF/HOLD spells
6. **Multi-phase**: Morgoth - stats scale with damage taken

### Recommendations
1. **Balcmeg** should have evasion +6 or +7 (matching or exceeding Orc Champion) to feel unique
2. **Dagorhir** could use +20% HP or +1d4 protection to better represent "Elfbane" status
3. **Gostir** could gain +2 evasion or +1d4 protection to compensate for lack of breath weapon damage
4. Consider adding more "speed 3" uniques in the depth 15-18 range to bridge the gap between speed 2 and the depth 20+ speed demons

### Conclusion
Overall, **90% of uniques are well-balanced** and appropriately powerful compared to general monsters of the same type and depth. The unique designs show creativity in differentiation (glass cannons, tanks, speedsters, fear specialists, leaders). The power scaling from early-game (Gorgol) to end-game (Morgoth) is excellent.

The few underpowered uniques (Balcmeg, Dagorhir, Gostir) could use minor stat buffs to better justify their unique status, but they're not gamebreaking issues - just opportunities for polish.
