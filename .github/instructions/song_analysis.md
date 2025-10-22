# Song System Technical Analysis

## Overview
The song system in Sil-QH allows players to sing magical songs that provide various effects. Songs cost voice (mana/csp) and can be maintained across multiple turns. The system supports "Woven Themes" which allows singing two songs simultaneously (major + minor theme at half effectiveness).

## Song List (20 Songs)

### 1. Song of Elbereth (SNG_ELBERETH, #140)
- **Level**: 1
- **Effect**: Causes fear in nearby servants of Morgoth
- **Voice Cost**: 1 per turn
- **Mechanics**:
  - Skill check: Song vs (Monster Will + distance)
  - Only affects intelligent monsters (RF2_SMART)
  - Morgoth is immune
  - Reduces monster temporary morale by result * 10
  - Has lingering effect that persists after stopping (song_elbereth_effect)
- **Ability Bonus**: Full skill value

### 2. Song of Challenge (SNG_CHALLENGE, #141)
- **Level**: 1
- **Effect**: Weakens Will, causes aggressive stance, applies stealth penalty
- **Voice Cost**: 1 per 3 turns
- **Mechanics**:
  - Skill check: Song vs (Monster Will² / 10 + distance)
  - Increases monster alertness by result
  - Sets monster temp_morale to minimum 30
  - Forces recalculation of stance (may turn aggressive)
  - Has lingering effect (song_challenge_effect)
  - Stealth penalty applied separately in noise calculation
- **Ability Bonus**: Full skill value
- **Notes**: Square-rooting resistance makes it more effective against low-Will monsters

### 3. Song of Delvings (SNG_DELVINGS, #142)
- **Level**: 2
- **Effect**: Reveals terrain around you, expanding from known areas
- **Voice Cost**: 1 per 3 turns
- **Mechanics**:
  - Range: score + 10
  - Maps squares within Manhattan distance 2 of known squares
  - Gradually expands mapped area outward
  - Also affects dungeon mapping flow
- **Ability Bonus**: Full skill value

### 4. Song of Freedom (SNG_FREEDOM, #143)
- **Level**: 2
- **Effect**: Discovers/overcomes doors, traps, rubble; grants freedom of movement
- **Voice Cost**: 1 per 3 turns
- **Mechanics**:
  - Base difficulty: depth/2 (min 10)
  - Unlocks/disarms chests (pval > 0)
  - Opens closed/locked doors
  - Reveals/disarms floor traps
  - Removes rubble
  - Grants free_act +1 (in calc_bonuses)
  - Reduces slow counter each turn in process_world
- **Ability Bonus**: Full skill value
- **Bug**: No distance modifier - all targets use same base difficulty

### 5. Song of Silence (SNG_SILENCE, #144)
- **Level**: 3
- **Effect**: Dampens sounds, makes detection harder
- **Voice Cost**: 1 per 3 turns
- **Mechanics**:
  - Adds 0 to song noise (doesn't increase stealth penalty from singing)
  - Reduces monster song skill by (player_song_skill / 4) when monsters sing
  - Prevents various sound-based effects
- **Ability Bonus**: skill / 2
- **Bug**: When countering monster songs, applies penalty of ability_bonus(SNG_SILENCE)/2 = skill/4, not full skill

### 6. Song of Staunching (SNG_STAUNCHING, #145)
- **Level**: 3
- **Effect**: Stops bleeding, speeds healing
- **Voice Cost**: 1 per turn
- **Mechanics**:
  - Immediately sets cut to 0
  - Healing: (score / 12) + fractional healing based on (duration % 12)
  - Fractional formula: if ((cycle * song_frac) % 12 < song_frac) add 1 HP
  - Very reliable passive healing
- **Ability Bonus**: Full skill value

### 7. Song of Thresholds (SNG_THRESHOLDS, #146)
- **Level**: 4
- **Prerequisites**: Elbereth or Delvings
- **Effect**: Wards closed doors against enemies
- **Voice Cost**: 1 per 3 turns
- **Mechanics**:
  - Triggered when player closes a door (cmd2.c)
  - Skill check: Song vs difficulty (15 base, 0 with UNQ_SNG_MEL house bonus)
  - Creates glyph of warding on the door
- **Ability Bonus**: Full skill value

### 8. Song of the Trees (SNG_TREES, #147)
- **Level**: 5
- **Prerequisites**: Elbereth AND Staunching
- **Effect**: Increases light radius
- **Voice Cost**: 1 per 3 turns
- **Mechanics**:
  - Bonus radius: 1 + (score / 5)
  - Calls light_area() with 1+(score/10) dice of score sides
  - Applied as p_ptr->cur_light bonus in calc_bonuses
- **Ability Bonus**: skill / 5
- **Bug**: Mismatch between ability_bonus (skill/5) and actual implementation (1 + score/5)

### 9. Song of Revealing (SNG_REVEALING, #148)
- **Level**: 7
- **Prerequisites**: Delvings
- **Effect**: Reveals monsters using Song skill instead of Listen; senses objects
- **Voice Cost**: 1 per 3 turns
- **Mechanics**:
  - Uses Song skill for detect_monster_noise() instead of Listen
  - Range: score + 10
  - Permanently marks objects within range
  - Marks mimic monsters (those with item-like d_char: |!?-_=~)
  - Minor theme gets halved skill for detection
- **Ability Bonus**: Full skill value

### 10. Woven Themes (SNG_WOVEN_THEMES, #149)
- **Level**: 6
- **Prerequisites**: Elbereth AND Freedom
- **Effect**: Allows second song (minor theme) at half Song score
- **Voice Cost**: N/A (not a song itself)
- **Mechanics**:
  - Changes song selection behavior in change_song()
  - Minor theme stored in song2
  - All ability_bonus calculations check if song == song1, else divide skill by 2
  - Both songs paid for separately in sing()
- **Ability Bonus**: N/A
- **Bug**: ability_bonus logic uses "song1 != abilitynum" which could incorrectly halve major theme in some contexts

### 11. Song of Slaying (SNG_SLAYING, #150)
- **Level**: 6
- **Prerequisites**: Challenge AND Freedom
- **Effect**: Critical hits kill outright if enemy HP ≤ 2× Song score
- **Voice Cost**: 1 per turn
- **Mechanics**:
  - Checked in critical hit code (cmd1.c)
  - If crit_bonus_dice > 0 AND m_ptr->hp <= ability_bonus(S_SNG, SNG_SLAYING)
  - Instant kill, sets dam = m_ptr->hp
- **Ability Bonus**: skill × 2
- **House Bonus Bug**: ability_bonus checks UNQ_SNG_HURIN but applies to SNG_STAYING, not SNG_SLAYING

### 12. Song of Elveness (SNG_ELVENESS, #151)
- **Level**: 7
- **Prerequisites**: Trees
- **Effect**: +1 Grace, +1 Evasion (+ 1 per 7 Song)
- **Voice Cost**: 1 per turn
- **Mechanics**:
  - Grace bonus applied in calc_bonuses: +1 while singing
  - Evasion bonus: 1 + (skill / 7)
- **Ability Bonus**: Full skill value

### 13. Song of Staying (SNG_STAYING, #152)
- **Level**: 7
- **Prerequisites**: Challenge AND Staunching
- **Effect**: Bonus to Will equal to half Song score; +[2d2] protection
- **Voice Cost**: 1 per turn
- **Mechanics**:
  - Will bonus applied in calc_bonuses
  - With UNQ_SNG_FIN house bonus: full Song score to Will
  - Without: half Song score to Will
  - Protection: standard [2d2] dice roll
- **Ability Bonus**: skill (×2 with UNQ_SNG_HURIN - but this is wrong!)
- **Bug**: House bonus UNQ_SNG_HURIN should apply to Slaying, not Staying

### 14. Song of Disguise (SNG_DISGUISE, #153)
- **Level**: 8
- **Prerequisites**: Silence
- **Effect**: Glamour - monsters mistake you for ally unless they pierce it
- **Voice Cost**: 2 per turn
- **Mechanics**:
  - Player skill: Song + Will
  - Monster difficulty: Monster Will + Perception - (distance-1)
  - Penalties: +5 per other watcher, +10 if already seen, +5 per attacker last turn
  - Success: monster becomes "pacified", removed from "seen" list
  - Failure: monster added to "seen" list
  - Complex state tracking with song_disguise_seen[], song_disguise_pacified[], etc.
  - Cleared when stopping song or on new level
- **Ability Bonus**: skill + 5

### 15. Song of Lorien (SNG_LORIEN, #154)
- **Level**: 8
- **Prerequisites**: Silence AND Delvings
- **Effect**: Gradually puts nearby opponents to sleep
- **Voice Cost**: 1 per turn
- **Mechanics**:
  - Skill check: Song vs (Monster Will + 5 + distance)
  - Success: reduces alertness by result
  - Immune if NO_SLEEP flag
  - UNQ_SNG_LUT house bonus: doubles Song score (2×score)
- **Ability Bonus**: Full skill value

### 16. Song of Shattering (SNG_SHATTERING, #155)
- **Level**: 9
- **Prerequisites**: Slaying
- **Effect**: Shatters enemy gear (weapons/armor)
- **Voice Cost**: 2 per turn
- **Mechanics**:
  - Only affects monsters with HAS_WEAPON or HAS_ARMOUR flags
  - Skill check: Song vs (Monster Will + distance)
  - 50/50 choice between weapon and armor (if both possible)
  - Probability to weaken: score/5 percent (4% at score=20!)
  - Weapon: reduces blow_ds by 1 (max reduction: ds-1)
  - Armor: reduces armor_ps by 1 (max reduction: race ps)
  - Also shatters floor items (weapons/armor) within range
- **Ability Bonus**: Full skill value
- **Bug**: Probability score/5 seems very low for high-level song

### 17. Song of Mastery (SNG_MASTERY, #156)
- **Level**: 10
- **Prerequisites**: Freedom AND Silence
- **Effect**: Occasionally prevents nearby opponents from moving/acting
- **Voice Cost**: 2 per turn
- **Mechanics**:
  - Applied in make_attack_normal (melee2.c)
  - Roll: 2d8 + ability_bonus(S_SNG, SNG_MASTERY)
  - Check vs monster Will
  - Success: monster skips turn (mflag |= MFLAG_SINGING)
  - UNQ_SNG_THINGOL house bonus: doubles Song score
- **Ability Bonus**: skill (×2 with UNQ_SNG_THINGOL)

### 18. Song of Contest (SNG_CONTEST, #158)
- **Level**: 12
- **Prerequisites**: Staying
- **Effect**: Personal duel system - contest of will that builds stacks
- **Voice Cost**: 7 per turn
- **Mechanics**:
  - Must target single foe at start (target_set_interactive)
  - Player skill: Song + (Will / 2)
  - Success: Builds monster stacks (max 3)
  - Failure: Builds player stacks (max 3)
  - Tie: No change
  - At 3 stacks: permanent penalties applied
    - Monster loses: -Will (skill/3), -Stealth (skill/2), -Evasion (skill/5), -Armor (skill/12)
    - Player loses: random stat decrease
  - Lockout timer: 10 turns after duel ends
  - Stack decay: 10 turns if no progress
- **Ability Bonus**: Full skill value

### 19. Song of Lament (SNG_LAMENT, #159)
- **Level**: 15
- **Prerequisites**: Lorien
- **Effect**: Saps spirit, inflicts permanent frailty
- **Voice Cost**: 7 per turn
- **Mechanics**:
  - Must target single foe at start
  - Player skill: Song + (Will / 2)
  - Success: Builds monster stacks (max 3)
  - Failure: Resets monster stacks
  - At 3 stacks: permanent penalties applied
    - -Will (skill/2)
    - -HP max (10/12 per stack, min 1)
    - -Damage dice (skill/12 reduction to blow dd)
    - Player loses 1 Grace point
  - Lockout timer: 10 turns after duel ends
- **Ability Bonus**: Full skill value

### 20. Grace (SNG_GRA, #157)
- **Level**: 12
- **Effect**: Gain a point of Grace
- **Voice Cost**: N/A (not a song)
- **Mechanics**: Direct stat increase, not an active ability

## Voice (Mana) Cost Patterns

### Every Turn (Heavy)
- Elbereth: 1
- Staunching: 1
- Elveness: 1
- Staying: 1
- Slaying: 1
- Lorien: 1

### Every 3rd Turn (Light)
- Challenge: 1 per 3 turns
- Delvings: 1 per 3 turns
- Freedom: 1 per 3 turns
- Silence: 1 per 3 turns
- Thresholds: 1 per 3 turns
- Trees: 1 per 3 turns
- Revealing: 1 per 3 turns

### Heavy Cost
- Disguise: 2 per turn
- Shattering: 2 per turn
- Mastery: 2 per turn
- Contest: 7 per turn (only major theme)
- Lament: 7 per turn (only major theme)

### Cost Formula
```c
if ((p_ptr->song_duration % 3) == type - 1)
    cost += 1;
```
Where type = 1 for major theme, type = 2 for minor theme. This staggers costs between major and minor themes.

## House Bonuses

### UNQ_SNG_FIN (Finarfin)
- Song of Staying: Twice as effective Will bonus (full score instead of half)

### UNQ_SNG_LUT (Lúthien)
- Song of Lorien: Twice as effective (2× score in skill check)

### UNQ_SNG_MEL (Melian)
- Song of Thresholds: Reduced difficulty (0 instead of 15)

### UNQ_SNG_HURIN (Húrin)
- **BUG**: Applied to SNG_STAYING in ability_bonus, should be SNG_SLAYING

### UNQ_SNG_THINGOL (Thingol)
- Song of Mastery: Twice as effective

## Bugs Identified

### 1. House Bonus Misapplication (Critical)
**Location**: `src/xtra1.c:2123`
```c
case SNG_STAYING:
{
    bonus = ((c_info[p_ptr->phouse].flags_u & UNQ_SNG_HURIN) ? 2 : 1) * skill;
    break;
}
```
**Issue**: UNQ_SNG_HURIN should apply to SNG_SLAYING based on the flag name and character background (Húrin's children Turin and Nienor were dragon-slayers).

**Fix**: Move the house bonus check to SNG_SLAYING case.

### 2. Song of Silence Counter Penalty (Minor)
**Location**: Multiple locations in `spells1.c`
```c
if (singing(SNG_SILENCE))
    song_skill -= ability_bonus(S_SNG, SNG_SILENCE) / 2;
```
**Issue**: ability_bonus(S_SNG, SNG_SILENCE) already returns skill/2, so dividing by 2 again gives skill/4 penalty instead of intended full skill penalty.

**Fix**: Remove the "/ 2" when applying to monster songs.

### 3. Woven Themes Penalty Logic (Minor)
**Location**: `src/xtra1.c:2063`
```c
if (skilltype == S_SNG)
{
    // penalize minor themes
    if (p_ptr->song1 != abilitynum)
        skill /= 2;
```
**Issue**: This check means "if checking an ability that's not the major theme, halve it". But this could incorrectly halve the major theme if called in a context where song1 happens to differ from the ability being checked.

**Fix**: Check explicitly for song2: `if (p_ptr->song2 == abilitynum && p_ptr->song1 != abilitynum)`

### 4. Song Duel Target Persistence (Critical)
**Location**: Monster death handling
**Issue**: When a monster dies while targeted by Contest or Lament, the player's song_target_idx isn't cleared, potentially causing crashes.

**Fix**: Add cleanup in delete_monster_idx or monster death handling.

### 5. Song of Shattering Low Probability (Balance)
**Location**: `src/spells1.c:6708, 6723`
```c
int weaken_chance = score / 5;
if (percent_chance(weaken_chance))
```
**Issue**: At typical max score of 20, this gives only 4% chance. Seems too low for a level 9 song costing 2 voice/turn.

**Fix**: Consider score/3 (6.7% at 20) or even score/2 (10% at 20).

### 6. Song of Freedom No Distance Scaling (Minor)
**Location**: `src/spells1.c:5166-5300`
**Issue**: All doors/traps use same base difficulty regardless of distance from player.

**Fix**: Add distance modifier like other songs: `difficulty = base_difficulty + flow_dist(FLOW_PLAYER_NOISE, y, x)`

### 7. Lingering Effects Double-Application (Design)
**Location**: Multiple
**Issue**: Song of Challenge and Song of Elbereth have lingering effect counters that are set every turn while singing, but the actual debuffs are also applied every turn. This creates a persistent debuff that lasts after stopping.

**Fix**: Either remove lingering effects or only apply debuff from lingering counter, not both.

### 8. Missing Flow Updates (Performance)
**Issue**: Songs like Elbereth and Challenge use flow_dist() but don't call update_flow() first, relying on existing flow data which may be stale.

**Fix**: Call update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE) at start of song effect functions.

## Implementation Quality

### Well-Implemented Songs
- **Staunching**: Clean healing math with fractional HP accumulation
- **Contest/Lament**: Sophisticated duel system with stack tracking and permanent penalties
- **Disguise**: Complex state machine for tracking which monsters see through glamour
- **Shattering**: Careful handling of multiple blow types and equipment

### Songs Needing Attention
- **Freedom**: Simplistic - no distance scaling
- **Thresholds**: Very simple, just creates glyph
- **Silence**: Inconsistent penalty application
- **Shattering**: Probability too low for cost

## Code Architecture

### Key Files
- `src/spells1.c`: Main song implementation (6000+ lines)
  - `sing()`: Main dispatcher, costs, calls individual song functions
  - `change_song()`: Song selection and UI
  - `singing()`: Check if song is active
  - Individual `sing_song_of_*()` functions
  - Song duel system functions
  - Song disguise state management

- `src/xtra1.c`: Bonus calculations
  - `ability_bonus()`: Returns effective skill/bonus for each song

- `src/cmd1.c`: Song of Slaying melee integration
- `src/cmd2.c`: Song of Thresholds door warding
- `src/dungeon.c`: Song tick processing, duel turn updates
- `src/melee2.c`: Song of Mastery AI, Challenge AI effects

### State Variables (player_type)
```c
byte song1;                          // Major theme
byte song2;                          // Minor theme (Woven Themes)
s16b song_duration;                  // Turns singing current song
s16b song_challenge_effect;          // Challenge lingering debuff
s16b song_elbereth_effect;           // Elbereth lingering debuff
int song_target_idx;                 // Duel target monster index
int song_target_song;                // Which duel song active
byte song_contest_player_stacks;     // Player stacks in Contest
s32b song_contest_last_turn;         // Last turn stacks changed
byte song_lockout_timer;             // Turns until can sing duel again
```

### State Variables (monster_type)
```c
byte song;                           // Monster's current song
byte song_contest_stacks;            // Contest stacks
byte song_lament_stacks;             // Lament stacks
byte song_lockout_timer;             // Turns until can sing again
s32b song_contest_last_turn;         // Last Contest update
s32b song_lament_last_turn;          // Last Lament update
s16b song_will_penalty;              // Permanent Will reduction
s16b song_stealth_penalty;           // Permanent Stealth reduction
s16b song_evasion_penalty;           // Permanent Evasion reduction
byte song_armor_dice_penalty;        // Permanent armor reduction
```

### Monster Songs
- **SNG_BINDING**: Morgoth's song, slows player, locks doors
- **SNG_PIERCING**: Morgoth's mind-reading song
- **SNG_OATHS**: Gorthaur's song, summons oathwraiths

## Testing Recommendations

1. **House Bonus Verification**: Test each house with their bonus song to ensure correct application
2. **Woven Themes**: Test all combinations of major + minor themes, verify costs and effectiveness
3. **Song Duels**: Test Contest and Lament against various monster types, verify stack tracking and penalty application
4. **Disguise**: Test with multiple monsters, verify state tracking when monsters die/spawn
5. **Shattering**: Test probability feels appropriate for cost (currently very low)
6. **Slaying**: Verify HP threshold calculation with house bonus
7. **Silence Counter**: Test against monster songs, verify penalty is meaningful

## Performance Considerations

- Most songs iterate over all monsters (mon_max), acceptable for turn-based
- Disguise system allocates buffers for MAX_MONSTERS, cleaned up properly
- Flow distance calculations are cached per turn
- Song duration counter could overflow (s16b) after 32767 turns, but unlikely

## Save/Load

Song state is persisted including:
- Current songs (song1, song2)
- Song duration
- Lingering effect timers
- Duel state (target, stacks, lockout)
- Monster song penalties

Version check: VERSION_EXTRA 5 added song duel persistence data.
