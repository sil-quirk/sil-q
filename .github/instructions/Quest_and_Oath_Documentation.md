# Sil-Morë Quest and Oath System - Complete Documentation

## Table of Contents
1. [Oath System](#oath-system)
2. [Quest System](#quest-system)
3. [Quest Descriptions and Dialog](#quest-descriptions-and-dialog)
4. [Special Abilities](#special-abilities)
5. [Implementation Details](#implementation-details)

---

## Oath System

### Overview
Oaths are vows that can be sworn at character creation, providing both benefits and restrictions to gameplay. Each oath grants specific bonuses but imposes limitations that, if broken, result in penalties.

### Available Oaths

#### 1. Oath of Mercy
- **Tolkien Quote**: "Let no blood of the Children stain thy blade in these halls of sorrow"
- **Restriction**: You may not attack Men or Elves
- **Reward**: +1 Grace
- **Unlock Condition**: Complete Niena's Mercy Quest
- **Oath Flag**: `OATH_MERCY_FLAG`

#### 2. Oath of Silence  
- **Tolkien Quote**: "In silence came I, and in silence shall I depart, as befits the wise"
- **Restriction**: You may not sing
- **Reward**: +1 Strength
- **Unlock Condition**: Complete Tulkas Quest
- **Oath Flag**: `OATH_SILENCE_FLAG`

#### 3. Oath of Iron
- **Tolkien Quote**: "Though darkness gather and Balrogs rise, I shall not yield nor turn aside"
- **Restriction**: You may not ascend without a Silmaril
- **Reward**: +2 Constitution
- **Unlock Condition**: Complete Mandos Quest
- **Oath Flag**: `OATH_IRON_FLAG`

#### 4. Oath of the Smith
- **Tolkien Quote**: "By mine own hand shall all blades be wrought, and no other's craft shall I bear"
- **Restriction**: You may not pick up weapons or armour from the ground
- **Reward**: +5 Smithing
- **Unlock Condition**: Complete Aule Quest
- **Oath Flag**: `OATH_SMITH_FLAG`

### Oath Breaking
When an oath is broken:
- All oath-derived stat bonuses are immediately removed
- A curse may be applied depending on the oath
- The oath becomes permanently banned for the remainder of the current metarun
- The character is forever marked as an oathbreaker

#### Breaking Conditions
Each oath has specific conditions that, if violated, will break the oath:

- **Oath of Mercy**: Attacking or dealing damage to Men or Elves
- **Oath of Silence**: Using songs or vocal abilities
- **Oath of Iron**: Ascending stairs without possessing a Silmaril
- **Oath of the Smith**: Picking up weapons or armor from the ground (not self-crafted)

#### Oath Breaking Process
When an oath is about to be broken, the player receives a confirmation prompt:
*"Are you sure you wish to break your oath?"*

If the player confirms:
1. **Immediate Consequences**:
   - The corresponding special ability is deactivated
   - All stat bonuses from the oath are removed
   - A message is logged in the character notes

2. **Curse Selection**:
   - Player chooses 1 curse from 3 random options using the escape curse interface
   - If selection fails, a random curse is applied automatically
   - Message: *"The weight of broken faith burdens you with an ancient curse!"*

3. **Permanent Consequences**:
   - The oath is banned for the rest of the current metarun
   - Message: *"Your oath of [name] is forever broken in this age."*
   - Character screen displays oath status as "(Broken)"

#### Oathbreaker Status
Once an oath is broken:
- Character displays show the oath as "(BROKEN)" in red text
- At character death/escape: *"You will be remembered always as a shameful oathbreaker."*
- Birth screen for future characters shows banned oaths with menacing text:
  
  ```
  OATH BROKEN
  "Thy oath lies shattered,
   thy word worthless as dust."
  
  "No Valar shall hear thy voice,
   no light shall guide thy path."
  
  Forever marked as oathbreaker
  in this age.
  ```

#### Metarun Persistence
- Broken oaths cannot be selected again in any future characters within the same metarun
- Only affects the current metarun - future metaruns allow oath selection again
- Oath unlocks remain available; only the "banned" status persists
- A note is added to the character log
- Special effects may occur (e.g., `p_ptr->oaths_broken |= OATH_IRON_FLAG`)

---

## Quest System

### Overview
The quest system features four main Valar quest givers, each offering unique challenges and rewards. Completing quests unlocks corresponding oaths for future characters in the metarun.

### Quest Givers and Mechanics

#### 1. Tulkas the Strong (Combat Quest)
- **Monster ID**: R_IDX_TULKAS (18)
- **Appearance**: V:Y (Yellow Valar symbol)
- **Quest Type**: Combat challenge
- **Spawn Condition**: Entrance-based spawning
- **Objective**: Defeat a specific monster assigned by Tulkas
- **Reward**: Legendary artifact weapon
- **Unlocks**: Oath of Silence

**Quest Dialog**:
```
"Tulkas the Strong speaks in a voice like thunder: 'Champion of valor!'"
"'I have watched your battles in these dark halls, and your courage stirs my heart.'"
"'If you would prove yourself worthy of my blessing, show me your mettle in combat.'"
"'Slay [monster name] and return to claim your prize.'"
"'When this deed is done, you shall be rewarded with [artifact name].'"
```

**Completion Dialog**:
```
"Tulkas appears with a great laugh of triumph!"
"'Well fought, warrior! You have proven your valor in battle.'"
"'Take this gift, forged in the deeps of time before the world's making.'"
"Tulkas strides away with thunderous footsteps, leaving your prize behind."
```

#### 2. Mandos the Doomsman (Justice Quest)
- **Monster ID**: R_IDX_MANDOS (20)
- **Appearance**: V:D (Dark Valar symbol)
- **Quest Type**: Specific target elimination
- **Objective**: Slay Brodda the Easterling (Aldor)
- **Reward**: Mandos' Doom special ability (fear resistance)
- **Unlocks**: Oath of Iron

**Quest Dialog**:
```
"Mandos speaks with the authority of the Valar:"
"'Mortal, you have descended deep into the darkness."
"Here lies Brodda the Easterling, a cruel tyrant who oppressed"
"the people of Dor-lómin and brought suffering upon the Edain.'"
"'It was foretold that Túrin Turambar would slay him in righteous"
"vengeance, but fate has been altered. Now this burden falls to you.'"
"'Slay Brodda, and you shall be granted passage deeper into"
"the halls of Mandos, where greater trials await.'"
```

**Completion Dialog**:
```
"Brodda the Easterling falls! His tyranny is ended at last."
"The spirits of Dor-lómin can finally know peace."
"Return to Mandos the Doomsman to claim your reward."

"Mandos acknowledges you with respect:"
"'You have fulfilled the task set before you."
"'Accept the gift of my doom - protection from the fears that plague mortals.'"
```

#### 3. Aule the Smith (Crafting Quest)
- **Monster ID**: R_IDX_AULE (19)
- **Appearance**: V:u (Aule Valar symbol)
- **Quest Type**: Smithing challenge
- **Objective**: Demonstrate crafting skill at Aule's forge
- **Reward**: Enhanced smithing abilities
- **Unlocks**: Oath of the Smith

**Quest Dialog**:
```
"Aule speaks in a voice like hammer on anvil:"
"'Mortal, I have watched your progress through these halls.'"
"'If you would prove worthy of my blessing, you must demonstrate'"
"your skill at the forge. Show me what you can create.'"
"'The forge awaits your skill. Show me what you can create.'"
```

#### 4. Niena, Lady of Pity (Mercy Quest)
- **Monster ID**: R_IDX_NIENA (260)
- **Appearance**: V:v1 (Light violet Valar symbol)
- **Quest Type**: Pacifist challenge
- **Spawn Condition**: Maximum-size levels (55x165) with sufficient stair separation
- **Objective**: Reach down stairs without killing any monsters
- **Reward**: Enhanced Stealth ability based on mercy shown
- **Unlocks**: Oath of Mercy

**Quest Dialog**:
```
"Niena, Lady of Pity, speaks with a voice full of sorrow and hope:"
"'I have seen too much suffering in these halls of stone.'"
"'The creatures here are lost and tormented, driven by fear and darkness.'"
"'If you can find mercy in your heart, I ask you this:'"
"'Reach the stairs downward without taking any life.'"
"'Show that strength can be wedded to compassion.'"
"'All stairs shall be revealed to guide your path.'"
```

**Quest Mechanics**:
- All stairs on the level become visible when quest is accepted
- Monster counter tracks seen vs. killed
- Stealth bonus = 10 * (monsters_seen - monsters_killed) / monsters_seen
- Quest fails if player leaves level via any stairs during active quest

**Completion Dialog**:
```
"As you step onto the stairs, you feel Niena's presence return."
"'You have done well, showing mercy where others would show only violence.'"
"Wait here a moment - she wishes to speak with you."

"Niena appears with tears of joy in her eyes!"
"'You have shown true compassion in these dark halls.'"
"'Your mercy toward the lost creatures is a light in the darkness.'"
"'Accept this blessing - may your footsteps be ever silent when mercy guides your path.'"
"Niena smiles sadly and fades away, leaving you with her blessing."
```

**Quest Warning System**:
When attempting to leave level during active Niena quest:
```
"Niena's voice echoes in your mind:"
"'If you leave now, you will have failed the mercy quest.'"
"'All the compassion you have shown will be for naught.'"
"Are you sure you wish to abandon the quest and [ascend/descend]? "
```

---

## Special Abilities

### Quest-Granted Abilities

#### Mandos' Doom (SPC_MANDOS)
- **Source**: Completing Mandos Quest
- **Effect**: Resistance to fear effects
- **Description**: Protection from the fears that plague mortals

#### Niena's Mercy (SPC_NIENA_MERCY)
- **Source**: Completing Niena Quest
- **Effect**: Enhanced Stealth based on mercy shown
- **Formula**: Stealth bonus = 10 * (seen_monsters - killed_monsters) / seen_monsters
- **Description**: Silent footsteps when mercy guides your path

---

## Implementation Details

### Quest States

#### Universal Quest States
```c
#define QUEST_NOT_STARTED    0
#define QUEST_GIVER_PRESENT  1  // Quest giver spawned but not activated
#define QUEST_ACTIVE         2  // Quest accepted and in progress
#define QUEST_SUCCESS        3  // Quest completed but reward not claimed
#define QUEST_REWARDED       4  // Quest fully completed
```

#### Specific Quest Variables
- `p_ptr->tulkas_quest` - Tulkas quest state
- `p_ptr->mandos_quest` - Mandos quest state  
- `p_ptr->aule_quest` - Aule quest state
- `p_ptr->niena_quest` - Niena quest state

### Quest Data Tracking
- `p_ptr->tulkas_target_r_idx` - Monster to defeat for Tulkas
- `p_ptr->tulkas_prize_a_idx` - Artifact reward from Tulkas
- `p_ptr->niena_monsters_seen` - Monster count for mercy tracking
- `p_ptr->niena_monsters_killed` - Kill count for mercy tracking

### Metarun Integration
Quests unlock oaths for future characters:
- `metarun_unlock_oath(OATH_MERCY)` - Unlocks Mercy oath
- `metarun_unlock_oath(OATH_SILENCE)` - Unlocks Silence oath
- `metarun_unlock_oath(OATH_IRON)` - Unlocks Iron oath
- `metarun_unlock_oath(OATH_SMITH)` - Unlocks Smith oath

### Level Generation Integration
- **Entrance-based**: Tulkas spawns near up stairs on appropriate levels
- **Size-based**: Niena spawns on maximum-size levels (l >= 5) with stair separation >= half diagonal
- **Depth-based**: Mandos and Aule have specific depth and encounter conditions
- **Quest Reservation**: `p_ptr->quest_reserved[0]` prevents multiple quest spawns per level

### File Locations
- **Quest Logic**: `src/xtra2.c` (interaction functions)
- **Oath Definitions**: `src/cmd4.c` (oath arrays and descriptions)
- **Birth Screen**: `src/birth.c` (oath selection interface)
- **Monster Definitions**: `lib/edit/monster.txt` (quest giver monsters)
- **Character Descriptions**: `lib/edit/character.txt` (character lore)

---

## Notes

### Design Philosophy
The quest and oath system is designed to:
1. Provide meaningful character customization through sworn restrictions
2. Encourage different playstyles (combat, stealth, crafting, pacifism)
3. Integrate Tolkien lore through appropriate challenges
4. Reward completion with both mechanical benefits and unlocked options
5. Create persistent progression across multiple characters in a metarun

### Future Considerations
- Additional Valar quest givers could be added following the established pattern
- Oath breaking consequences could be expanded
- Quest difficulty scaling based on depth or character level
- Multiple quest per level with more complex reservation systems

---

*This documentation covers the complete quest and oath system as implemented in Sil-Morë, including all dialog text, mechanical details, and integration points.*
