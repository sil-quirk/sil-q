# Quest Text Storage System Design Plan

## Overview
This document outlines a plan to store all quest texts, rewards, and associated oaths in text files similar to character.txt, following the existing game's data architecture.

## Current Character.txt Structure Analysis

### Character.txt Format
```
N:character_number:character_name       # Character entry header
A:alternate_name                        # Alternate name form
B:brief_welcome_string                  # Welcome text when selected
F:character_flags                       # Flags affecting skills/abilities
U:unique_flag                          # Special unique character abilities
S:str:dex:con:gra                      # Stat bonuses
C:abilities:numbers                     # Abilities granted
P:number                               # Priority/points
D:description_line                      # Multi-line description text
```

### Key Patterns Observed
1. **Entry Headers**: Use `N:` with sequential numbering
2. **Text Storage**: Multi-line descriptions with `D:` prefix
3. **Rewards**: Stored as separate fields (S: for stats, C: for abilities, F: for flags)
4. **Data Types**: 
   - Stats: `S:str:dex:con:gra` (numeric bonuses)
   - Abilities: `C:ability_id:ability_id:...` (ability references)
   - Flags: `F:FLAG1|FLAG2|FLAG3` (flag combinations)
   - Skills: Handled through flags with affinity/penalty system

## Proposed Quest Text Storage System

### File Structure: quest.txt
```
# File: quest.txt
#
# This file stores quest texts, challenges, rewards, and associated oaths
# Format follows character.txt conventions for consistency

V:0.8.6  # Version stamp

# === QUEST DEFINITIONS ===

Q:0:Tulkas    # Quest ID:Quest Name
T:Quest of Tulkas the Strong                    # Quest Title
C:Defeat a randomly assigned enemy in battle   # Challenge Description
O:OATH_SILENCE                                 # Associated Oath
R:ARTIFACT                                     # Reward Type (ARTIFACT, STATS, ABILITIES, FLAGS, SKILLS)
Y:1                                            # Quest Type (0=vault-based, 1=roulet-based)
I:Tulkas the Strong speaks in a voice like thunder:
I:'Champion of valor!'
I:'Seek out %s, and prove your might by defeating this foe in battle.'
I:'When this deed is done, you shall be rewarded with %s.'
I:Tulkas grins fiercely and vanishes, leaving you with your sacred task.
W:Tulkas appears with a great laugh of triumph!
W:'Well fought, warrior! You have proven your valor in battle.'
W:'Take this gift, forged in the deeps of time before the world's making.'
W:Tulkas strides away with thunderous footsteps, leaving your prize behind.

Q:1:Aule      # Quest ID:Quest Name  
T:Quest of Aule the Smith
C:Forge an artifact with 4+ ego abilities
O:OATH_SMITH
R:STATS|ABILITIES
Y:0                                            # Quest Type (0=vault-based, 1=roulet-based)
S:0:0:0:0                           # Stat Rewards (none for Aule)
K:SMT:2                             # Skill Rewards (Smithing +2)
A:SPECIAL_ITEM                      # Special abilities/items granted
I:Aule, Lord of the Forge, speaks with the weight of mountains:
I:'Young smith, your hands show promise, but can they create true wonder?'
I:'Within this sacred forge, craft an artifact of at least four abilities.'
I:'Show me that mortal hands can shape the very essence of creation.'
I:'Only through such mastery will you earn the right to bear the Iron Oath.'
I:'Begin your work, and let the fires of creation guide your hammer.'
W:Aule nods with deep satisfaction, his eyes gleaming with approval.
W:'You have proven yourself worthy of the ancient ways of smithcraft.'
W:'Take this blade, forged in the fires of creation itself.'
W:'May your hands never forget what they have learned in my forge.'
W:The forge dims as Aule fades, leaving you with his blessing.

Q:2:Mandos    # Quest ID:Quest Name
T:Quest of Mandos the Doomsman  
C:Clear all enemies from the tomb
O:OATH_IRON
R:STATS|ABILITIES
Y:0                                            # Quest Type (0=vault-based, 1=roulet-based)
S:0:0:0:1                           # Stat Rewards (+1 Grace)
A:CURSE_REMOVAL                     # Special: Remove 1 curse
I:Mandos, Judge of the Dead, speaks in hollow, echoing tones:
I:'Mortal soul, you stand before the threshold of justice.'
I:'Within this tomb lie restless spirits, bound by ancient wrongs.'
I:'Clear them all, that they might find peace in the halls beyond.'
I:'Show mercy to the suffering, and justice shall be your reward.'
I:'Enter, and let righteousness guide your every action.'
W:Mandos appears, his presence bringing solemn peace to the chamber.
W:'Justice has been served, and mercy has found its mark.'
W:'Your compassion has freed those bound by ancient sorrows.'
W:'Accept this blessing, that grace might strengthen your noble heart.'
W:Mandos nods solemnly and fades, leaving blessed silence behind.

Q:3:Niena     # Quest ID:Quest Name
T:Quest of Niena the Mourner
C:Reach the stairs downward without taking any life  
O:OATH_MERCY                        # Correct oath - Mercy
R:ABILITIES
Y:1                                            # Quest Type (0=vault-based, 1=roulet-based)
A:SPC_NIENA_MERCY                   # Special ability granted
I:Niena, Lady of Pity, speaks with a voice full of sorrow and hope:
I:'I have seen too much suffering in these halls of stone.'
I:'The creatures here are lost and tormented, driven by fear and darkness.'
I:'If you can find mercy in your heart, I ask you this:'
I:'Reach the stairs downward without taking any life.'
I:'Show that strength can be wedded to compassion.'
I:'All stairs shall be revealed to guide your path.'
W:Niena appears with tears of joy in her eyes!
W:'You have shown that true strength lies in restraint.'
W:'Your mercy has been witnessed by all creation.'
W:'I grant you the gift of moving unseen, like shadows at dawn.'
W:Niena smiles sadly and fades away, leaving you with her blessing.
```

### Proposed Field Definitions

#### Quest Header Fields
- `Q:id:name` - Quest ID and internal name
- `T:title` - Full display title
- `C:challenge` - Challenge description for quest status menu
- `O:oath` - Associated oath (OATH_SILENCE, OATH_SMITH, OATH_IRON, OATH_MERCY, NONE)
- `R:reward_types` - Reward types (STATS, ABILITIES, FLAGS, SKILLS, ARTIFACT)
- `Y:type` - Quest type (0=vault-based like Aule/Mandos, 1=roulet-based like Tulkas/Niena)

#### Reward Fields
- `S:str:dex:con:gra` - Stat bonuses (same as character.txt)
- `K:skill:bonus` - Skill bonuses (e.g., "SMT:2" for +2 Smithing)
- `A:ability_id` - Abilities granted (ability IDs or special items)
- `F:flags` - Character flags granted
- `E:equipment` - Special equipment (for artifact descriptions)

#### Text Fields
- `I:text` - Initialization/quest giving text lines
- `W:text` - Reward/completion text lines
- `%s` placeholders - For dynamic content (monster names, artifact names)

### Oath System Extension

#### Separate Oath Section in quest.txt
```
# === OATH DEFINITIONS ===

O:0:SILENCE
N:Oath of Silence
D:You swear never to sing while this oath binds you.
R:STATS
S:1:0:0:0                           # +1 Strength
B:Singing breaks this oath and incurs curses.

O:1:SMITH  
N:Oath of the Smith
D:You swear never to wield a weapon you have not forged yourself.
R:STATS
S:0:0:2:0                           # +2 Constitution  
B:Wielding non-self-forged weapons breaks this oath and incurs curses.

O:2:IRON  
N:Oath of Iron
D:You swear never to flee from battle without a Silmaril.
R:STATS
S:0:0:2:0                           # +2 Constitution  
B:Fleeing without Silmaril breaks this oath and incurs curses.

O:3:MERCY
N:Oath of Mercy  
D:You swear never to kill helpless or sleeping foes.
R:STATS
S:0:0:0:1                           # +1 Grace
B:Killing helpless foes breaks this oath and incurs curses.
```

#### Oath Field Definitions
- `O:id:name` - Oath ID and internal name
- `N:display_name` - Full display name
- `D:description` - Oath description text
- `R:reward_type` - Type of reward granted
- `S:str:dex:con:gra` - Stat bonuses from oath
- `B:breaking_condition` - Text describing what breaks the oath

## Implementation Benefits

### 1. Consistency with Existing Architecture
- Follows established .txt/.raw file pattern
- Uses same parsing conventions as character.txt
- Integrates with existing data loading system

### 2. Maintainability
- Centralized text storage
- Easy to modify quest dialogues without recompiling
- Clear separation of quest logic and quest content

### 3. Extensibility  
- Easy to add new quests
- Simple reward system expansion
- Oath system can grow independently

### 4. Localization Ready
- All text in external files
- Multiple language support possible
- Consistent text formatting

## File Organization

### Directory Structure
```
lib/edit/
├── character.txt     # Existing character definitions
├── quest.txt         # New quest definitions  
├── oath.txt          # Optional: separate oath file
└── ...               # Other existing files

lib/data/
├── character.raw     # Existing compiled character data
├── quest.raw         # New compiled quest data
├── oath.raw          # Optional: compiled oath data  
└── ...               # Other existing compiled files
```

### Parser Integration
- Extend existing edit file parsing system
- Add quest.txt to compilation process
- Create quest_info[] array similar to r_info[], a_info[]
- Update initialization routines

## Migration Strategy

### Phase 1: Structure Creation
1. Create quest.txt file with current quest text
2. Define data structures for quest storage
3. Implement basic parsing functionality

### Phase 2: Code Integration  
1. Replace hardcoded quest text with data lookups
2. Update quest interaction functions
3. Modify quest status menu to use quest data

### Phase 3: Enhancement
1. Add oath system integration
2. Implement dynamic text substitution (%s placeholders)
3. Add validation and error checking

### Phase 4: Testing & Refinement
1. Verify all quest interactions work correctly
2. Test quest status menu accuracy
3. Validate oath integration
4. Performance testing and optimization

## Technical Considerations

### Memory Management
- Quest data loaded at startup like character data
- Minimal memory footprint (text stored efficiently)
- No dynamic allocation during gameplay

### Performance Impact
- Negligible - data pre-loaded and indexed
- Text lookup faster than hardcoded string comparisons
- Reduced code complexity in quest functions

### Compatibility
- Backward compatible with existing save files
- No changes to save/load format required
- Quest state storage remains unchanged

## Future Enhancements

### Advanced Features
1. **Conditional Text**: Different text based on character race/abilities
2. **Multi-stage Quests**: Support for quest chains
3. **Dynamic Rewards**: Rewards based on character level/progress
4. **Quest Prerequisites**: Requirements before quest availability

### Quality of Life
1. **Quest Journal**: Expanded quest tracking system  
2. **Hint System**: Progressive hints for stuck players
3. **Quest Analytics**: Track completion rates and difficulty
4. **Narrative Branching**: Multiple paths through quest content

This design provides a robust, extensible foundation for quest text storage while maintaining consistency with Sil-qh's existing architecture and supporting future enhancements.
