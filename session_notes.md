# Session Notes

## 2025-10-23 - Song of Shattering Debug Investigation

### Issue
Song of Shattering not applying debuffs - no messages in log and no visible effects in monster screen.

### Root Cause
**Song of Shattering was missing from the `ability_bonus()` function in `xtra1.c`!**

The song was properly integrated into the song processing loop, but when calculating the score with `ability_bonus(S_SNG, SNG_SHATTERING)`, it wasn't in the switch statement, so it returned a default bonus of 0.

From the log:
```
Song of Shattering: starting with score=0
Song of Shattering: skill_check result=-16 (score=0, resistance=13)
Song of Shattering: Attempting weapon damage, weaken_chance=0%
```

With score=0:
- All skill checks fail (0 vs monster Will + distance)
- Probability is 0/3 = 0% (should be score/3 percent)
- Song is completely ineffective

### Fix
Added `SNG_SHATTERING` case to the `ability_bonus()` function in `src/xtra1.c`:
```c
case SNG_SHATTERING:
{
    bonus = skill;
    break;
}
```

Placed after `SNG_MASTERY` and before `SNG_CONTEST` to maintain the logical ordering.

### Expected Behavior After Fix
With proper score calculation (e.g., skill 20 = score 20):
- Skill checks: 20 vs (monster Will + distance) - should pass for nearby orcs
- Probability: 20/3 = 6.7% chance per eligible monster per turn
- Messages should appear when equipment is damaged
- Monster screen should show reduced damage/armor values

### Testing
Close and restart the game to load the new executable, then:
1. Sing Song of Shattering near orcs with equipment
2. Check log.txt - should now show score > 0
3. Should see successful skill checks and occasional equipment damage

---

## 2025-10-23 - Song of Trees Fix

## Date
2025-10-23 (earlier)

## Summary
Fixed Song of Trees to damage/stun light-sensitive monsters silently without showing visual light effects like Gem of Light does.

### Issue
- Song of Trees was showing the same visual effect as Gem of Light ("You are surrounded by a white light.")
- Should increase light radius AND damage light-sensitive monsters, but WITHOUT the visual flash/message

### Root Cause (Updated after testing)
- Original implementation called `light_area()` which uses `PROJECT_GRID` flag
- `PROJECT_GRID` causes the visible light-up effect on dungeon squares
- `light_area()` also prints the "surrounded by white light" message
- Song of Trees should work silently in the background
- **Critical bug found:** `project()` reduces damage dice by 2 per square of distance by default
  - With `dd = 1 + (score/10)`, at score 10: dd=2
  - At distance 1: dd reduced to 0, no damage possible!
  - Fixed by using `uniform=true` parameter so dd doesn't decay

### Fix
- Modified `sing_song_of_trees()` to call `project()` directly instead of `light_area()`
- Uses flags: `PROJECT_BOOM | PROJECT_KILL | PROJECT_PASS | PROJECT_HIDE`
- Removed `PROJECT_GRID` to prevent visual lighting effect
- Added `PROJECT_HIDE` to suppress graphics
- **Set `uniform=true`** so damage dice don't decay with distance
- Damage/stun calculations use light level at monster's position, not distance from player
- Maintains damage/stun mechanics via GF_LIGHT handler (which checks `ds > 10` to identify Song vs Gem)

### Behavior After Fix
Song of Trees now:
1. ✅ Increases light radius passively (handled in xtra1.c:1989)
2. ✅ **Stuns HURT_LITE monsters reliably** based on light level (more consistent than damage)
3. ✅ Damages monsters only when Will check succeeds (requires bright light)
4. ✅ Works silently without visual effects or messages
5. ✅ Uses song score for damage skill checks (via `ds = score` parameter)

### Stun vs Damage Mechanics
**Stun (Primary Effect):**
- Calculated: `damroll(dd, light_level)` - scales with light level
- Applied when monster **fails Will save** (result > 0, player wins)
- Duration: Stun value in turns (decreases by 1 per turn)
- With light level 14, dd=2: **2-28 turns of stun**
- Represents the blinding/disorienting effect of light
- Orcs (Will 1-2) will almost always fail against high song skill

**Damage (Secondary Effect):**
- Only applied on **strong Will failure** (result ≥ 5)
- Calculated: `damroll(dd, light_level)` then reduced by resistance
- Reduction formula: `(damage × result) / (result + 5)`
- Represents actual burning/searing damage from intense light
- Bypasses armor (applied via `mon_take_hit`)

**Resistance Outcomes:**
- result ≥ 5: Stun + Damage ("is seared by radiant light!")
- result 1-4: Stun only ("cringes from the light!")
- result ≤ 0: Monster resists ("resists the light!")

This creates the intended progression:
1. Weak song / high monster Will: Monster resists
2. Moderate success: Monster stunned but not damaged (cringes)
3. Strong success: Monster stunned AND damaged (seared)

Gem/Staff of Light:
1. Shows "surrounded by white light" message
2. Creates visible light flash effect
3. Uses player's Will skill for damage checks

### Files Modified
- `src/spells1.c`: Lines 6652-6668 - replaced `light_area()` call with direct `project()` call using appropriate flags
- `src/spells1.c`: Lines 3418-3430 - added debug logging for damage calculation to diagnose issues

### Debug Logging
Added temporary debug logging to GF_LIGHT handler showing:
- Number of damage dice (dd)
- Light level at monster position
- Raw damage before Will reduction
- Will check result
- Final damage after Will reduction
- Stun amount applied

Check `sil-more-windows-sdl3/log.txt` for output.

### Technical Details
- Damage formula: `dd = 1 + (score/10)` dice of light level
- Radius: `rad = 1 + (score/5)`
- Skill parameter: `ds = score` (GF_LIGHT handler uses this to distinguish Song from Gem)
- Only affects monsters with HURT_LITE flag
- Damage reduced by monster Will resistance and distance
- **Critical requirement:** Damage only triggers when `cave_light[monster_pos] >= 3`
- Light sources provide different damage ranges:
  - Torch (radius 1): Never damages (max light = 2)
  - Lantern (radius 2): Damages same square only (light = 3)
  - Mallorn (radius 3): Damages up to 1 square away
  - Fëanorian (radius 4): Damages up to 2 squares away
  - Silmaril (radius 7): Damages up to 5 squares away

### Messages
When Song of Trees affects a HURT_LITE monster, you'll see:
- "[Monster] is seared by radiant light!" - when damage is dealt
- "[Monster] cringes from the light!" - when stunned but no damage
- "[Monster] resists the light!" - when Will save succeeds

These messages now display (removed PROJECT_SILENT flag) to provide feedback.

# Session Notes - Morgoth Crown Tiles


## Date
2025-10-27

## Summary
- Studied MicroChasm tile encoding (`attr & 0x3F` → row, `char & 0x3F` → column) via `graf-new.prf` and `callback_sdl_pict`.
- Added `object_attr_graphics_override()` / `object_char_graphics_override()` in `src/object1.c` to remap Morgoth crown artefacts once Silmarils are removed.
- Wired overrides into the `object_attr` / `object_char` macros (`src/defines.h`) and declared them in `src/externs.h` so all item renders honour the new tiles.
- Introduced shared tile helpers (`TILE_*`) and taught the pref parser/dumper about `R#/C#` row/column tokens so artists can work numerically instead of hex.

## Notes
- Crown with three Silmarils keeps existing tile (`0x85/0x9C`, row 5 col 28); variants now use row 12 with columns 23–25 while preserving glow/alert overlay bits.

# Session Notes - Woven Theme Synergy

## Date
2025-10-25

## Summary
- Added woven theme synergy handling so specified song pairs each gain +20% of base song skill when sung together.
- Included helper utilities in `src/xtra1.c` to detect synergy pairs and grant the shared bonus after applying minor theme penalties.

# Session Notes - Song Duels Mechanics Update

## Date
2025-10-24 (Evening)

## Summary
- Added Song of Contest and Song of Lament abilities after Grace with new data entries, enumerations (SNG_CONTEST, SNG_LAMENT, SNG_MAX), and song selection UI updates.
- Extended player/monster state for targeted songs: stored duel targets and stacks, stack timestamps, cooldown timers, permanent stat/armour/damage penalties, and saved them via VERSION_EXTRA 5 bump.
- Implemented targeted duel resolution each turn (Song+Will/2 vs monster Will), 7 voice upkeep, stack accrual/decay, and the on-3-stack outcomes (stat drains, grace loss, monster debuffs, singing lockouts).
- Introduced per-turn/cleanup hooks (song_duels_new_player_turn, song_duels_handle_monster_removed), enforced major-theme-only usage, and blocked monsters from starting songs while locked out.

# Session Notes - Morgoth Victory Update

## Date
2025-10-24 (Morning)

## Summary
- Added 10% health trigger for Morgoth's new desperate state with updated stats.
- Introduced dedicated Morgoth victory flow: new messaging, notes, and high-score handling via `do_cmd_morgoth_victory()`.
- Adjusted scoring, metarun, and blessing systems to reward Morgoth slayers (3 Silmarils awarded, doubled blessing pool contribution).

### Key Changes
1. Combat: `anger_morgoth()` state 5 stats now match design (60 attack, 10d10 damage, 40 evasion, 9d4 armour, increased Will/Per). `process_monster()` promotes Morgoth to state 5 precisely at 10% HP with new log/message hooks.
2. Victory Flow: `monster_death()` now triggers the victory sequence instead of the legacy bug banner, adds `do_cmd_morgoth_victory()` with note logging, and retitles tomb/final menu text for Morgoth slayers.
3. Meta & Scores: Morgoth victors are scored as if they ascended with all three Silmarils; blessing pool contributions are doubled; `metarun_update_on_exit()` has a new branch awarding +3 Silmarils and skipping kinslaying/treachery scenes.

# Session Notes - Metarun UI Improvements

## Date
2025-10-22 (Evening)

## Metarun Info Menu Enhancements - Round 3 (Final Alignment Fixes)

Fixed remaining alignment issues and blessing power visibility.

### Fixes Applied

1. **Perfect Column Alignment**
   - Changed from right-padding spaces to left-aligned format specifier: `%-8s`
   - "Blessing" and "Curse" now properly align in 8-character column
   - Fixed formatting: `%2d: %-28s %-8s %d - %s`

2. **H:/P: Visibility Fix**
   - H: (blessing power) and P: (curse power) now **only shown when identified** (`CURSE_SEEN()`)
   - Added `bool seen = CURSE_SEEN(id);` check before displaying effect
   - D: (description) still shown always as intended

### Build Status
✅ Compiled and deployed successfully

---

## Metarun Info Menu Enhancements - Round 2 (Bug Fixes)

Fixed alignment, encoding, and width issues based on testing feedback.

### Fixes Applied

1. **Blessing Pool Meter Encoding Fix**
   - Changed from Unicode box-drawing characters (╔═╗║) to simple ASCII (+|-|#)
   - Now uses `+----------+` for borders and `|##########|` for filled sections
   - Fixes garbled character display in terminal
   - Progress text format changed to compact: "113/350" instead of "113 / 350"

2. **Curse/Blessing List Alignment**
   - Fixed column alignment: `%2d: %-28s %s %d - %s`
   - "Blessing" and "Curse   " now properly aligned (8 chars each)
   - ID field: 2 digits, Name field: 28 chars fixed width
   - Removed extra indentation (was `col + 2`, now just `col`)
   - Effect text truncation respects meter position

3. **Terminal Width Handling**
   - Footer prompt now uses actual terminal width (minimum 80)
   - Calculates: `target_width = (term_width > 80) ? term_width : 80`
   - Pads footer to full width with spaces for clean display
   - Shortened prompt text to fit: "[b] Spend blessings  [f] Threshold  [c] Difficulty  [u] Full list  [s] History"
   - Footer starts at column 0 for full-width coverage

4. **Description Visibility (D: and E:)**
   - D: (curse description) and E: (blessing description) now **always shown**, even when not identified
   - P: (curse power) and H: (blessing power) still require identification (`CURSE_SEEN()`)
   - Changed message: "(Effect not yet identified)" instead of "(Not yet identified)"

5. **Width Calculations**
   - Main display respects meter position: `max_display_width = meter_col - 4`
   - Ensures 80-column minimum width compliance throughout
   - Text truncation with "..." when exceeding display area

### Build Status
✅ Compiled successfully with only pre-existing warnings
✅ Deployed to `sil-more-windows-sdl3/`

---

## Metarun Info Menu Enhancements - Round 1 (Initial Implementation)

Improved the metarun statistics and curse/blessing display screens with better layout, visual meter, and navigation.

### Changes Made

1. **Blessing Pool Meter** (`src/metarun.c`)
   - Added `draw_blessing_meter()` function that displays a vertical progress bar on the right side
   - Shows current blessing pool progress toward next point threshold in light blue (`TERM_L_BLUE`)
   - Uses box-drawing characters (╔═╗║╚╝) with filled blocks (████) for visual appeal
   - Displays progress ratio below the meter (e.g., "2450 / 5000")
   - Positioned at right edge (column = term_width - 16) to avoid overlap with main content

2. **Enhanced 'u' Menu - Full Effects List** (`src/metarun.c`)
   - Completely rewrote `show_all_active_curses()` to show both description and power for each effect
   - Now displays **both D: (description/flavor text) and H:/P: (mechanical effect)** for identified effects
   - Added pagination with left/right arrow navigation (keys 4/6) when effects don't fit on screen
   - Page indicator in title: "=== Active Effects (Page 1/3) ==="
   - Each effect shows: name, description, and mechanical effect on separate lines
   - Handles long text truncation with "..." for terminal width
   - 4 lines per effect (name + description + power + blank separator)

3. **Threshold Selection Menu Colors** (`src/metarun.c`)
   - Added color-coded difficulty modes in `adjust_blessing_threshold_menu()`
   - "Easier" mode: Green (`TERM_L_GREEN`)
   - "Normal" mode: White (`TERM_WHITE`)
   - "Harder" mode: Orange (`TERM_ORANGE`)
   - Highlighted selection shown in yellow (`TERM_YELLOW`)
   - Improved visual hierarchy with consistent color scheme

4. **Minimum 80-Column Width**
   - Ensured all text displays properly on minimum 80-width terminals
   - Footer prompt already padded to 80 characters
   - Blessing pool text simplified to fit within left column space
   - Layout calculations respect minimum width while adapting to larger terminals

### Technical Details

- Meter height: 15 rows on tall terminals, scales down to minimum 5 rows
- Meter column: `term_width - 16` (provides 14-char wide meter + borders)
- Navigation: Keys 4 (left) and 6 (right) for pagination, any other key exits
- Data sources: Uses `curse_type` fields - `text`/`blessing_text` for D:/E:, `power`/`blessing_power` for P:/H:
- Respects `CURSE_SEEN()` flag - only shows details for identified effects

### Build Status
- Compiled successfully with standard warnings (pre-existing type comparison issues)
- Deployed to `sil-more-windows-sdl3/` directory

---

# Session Notes - Song of Revealing Implementation

## Date
2025-10-22 (Morning)

## Song of Revealing

- Added Song of Revealing entry to `lib/edit/ability.txt` after Song of the Trees (ability id 8, skill req 7, prerequisite Song of Delvings) and renumbered later song abilities/prerequisites to keep ordering consistent.
- Introduced `SNG_REVEALING` enumeration between Trees and Woven Themes (`src/defines.h`), updated name table (`src/birth.c`), and granted it full skill scaling in `ability_bonus` (`src/xtra1.c`).
- Broke out shared noise-detection logic as `detect_monster_noise()` so Listen and the new song reuse the same checks (`src/monster2.c`, declaration in `src/externs.h`).
- Implemented `sing_song_of_revealing()` in `src/spells1.c` to run Song-skill-based monster reveals each turn and permanently mark nearby items via new `song_reveal_items()` helper; added start/maintenance messaging and voice cost handling.
- Ran the VS Code Build and Deploy workflow manually (`cmake` configure/build followed by `.vscode/deploy.ps1`) to produce and copy the updated SDL3 executable.

# Session Notes - Song Debuff Decay Implementation

## Date
2025-10-21

## Song of Challenge & Song of Elbereth - Gradual Debuff Decay

**Problem:** Song debuffs (Perception/Stealth penalty for Challenge, Will penalty for Elbereth) disappeared immediately when stopping the song, making tactical song-switching less viable.

**Solution:** Implemented gradual decay system using timed effect counters with skill-scaled duration.

### Files Modified

1. **src/types.h**
   - Added `s16b song_challenge_effect` - lingering debuff counter for Song of Challenge
   - Added `s16b song_elbereth_effect` - lingering debuff counter for Song of Elbereth
   - Placed after `tmp_per` to group with other timed effects

2. **src/spells1.c** (sing function)
   - `SNG_CHALLENGE`: Sets counter based on song skill: `duration = (skill × 3) / 4`
   - `SNG_ELBERETH`: Sets counter based on song skill: `duration = (skill × 3) / 4`
   - Minimum duration of 3 turns at low skill
   - At skill 20: 15 turns duration
   - At skill 10: 7 turns duration
   - At skill 30: 22 turns duration
   - Counter maintains while song is active, then decays naturally

3. **src/dungeon.c** (timed effect decay)
   - Added decay logic after temporary perception block
   - Each turn reduces counters by 1 until they reach 0
   - Duration varies based on song skill when effect was applied

4. **src/monster2.c** (monster_skill function)
   - Changed from checking `singing()` to checking effect counter `> 0`
   - Calculates max duration dynamically: `max_duration = (song_skill × 3) / 4`
   - Penalty scales linearly: `penalty = (full_penalty × current_effect) / max_duration`
   - Ensures minimum penalty of 1 while any effect remains
   - Enhanced debug logging shows `effect/max_duration` (e.g., "effect=8/15")

5. **src/save.c & src/load.c**
   - Added serialization for both new counters after `oppose_pois`
   - Reduced spare bytes from 19 to 15 (used 4 bytes: 2 × s16b)
   - Ensures save compatibility

### Mechanics

**Duration Scaling (skill-based):**
```
Skill  5 →  3 turns (minimum)
Skill 10 →  7 turns
Skill 15 → 11 turns
Skill 20 → 15 turns (baseline)
Skill 25 → 18 turns
Skill 30 → 22 turns
```

**Debuff Strength (example at skill 20):**
- Full strength (counter = 15/15): 100% penalty
- Half strength (counter = 8/15): ~53% penalty
- Quarter strength (counter = 4/15): ~27% penalty
- Final turn (counter = 1/15): Minimum penalty of 1

**Affected Skills:**
- Song of Challenge: Monster Will & Stealth (-1 to -2 typically)
- Song of Elbereth: Monster Will (-1 to -2 typically)

**Tactical Benefits:**
- Higher song skill = longer lingering protection
- Can switch songs without instant penalty loss
- Gradual fade prevents sudden difficulty spikes
- Scales naturally with character progression

### Build Status
✅ CMake build successful
✅ Deployment successful
✅ Save/load compatibility maintained
✅ Skill-based duration scaling implemented

### Critical Fix (2025-10-21)
**Issue:** Save files created after debuff decay update were failing to load with "Read savefile failed" error.

**Root Cause:** Save/load byte count mismatch
- `save.c` was writing 19 spare bytes (3 wr_byte + 4 wr_u32b = 3 + 16 = 19)
- `load.c` was reading 15 spare bytes with `strip_bytes(15)`
- This created a 4-byte misalignment causing subsequent reads to fail

**Fix:** 
- Removed one `wr_u32b(0L)` call in `save.c` to write only 15 spare bytes (3 + 12 = 15)
- Now matches the `strip_bytes(15)` in `load.c`
- Spare byte count correctly reflects: originally 19 bytes, used 4 for song debuff counters (2 × s16b), leaving 15

---

## Session Notes - Character song index fixes

- Date: 2025-10-22 (Evening)
- Fixed mismatched song ability indices in `lib/edit/character.txt` after the addition/reordering of Song abilities in `lib/edit/ability.txt`:
   - Updated Finrod's starting Song of Staying index to `11`.
   - Updated Lúthien's Song of Lorien index to `12`.
   - Corrected Daeron's Woven Themes index to `9`.
   - Corrected Melian's Song of Mastery index to `14`.
   - Adjusted a few other song references/comments to match `ability.txt` ordering.

Verification: performed an edit-only consistency pass on `lib/edit/character.txt` and confirmed no syntax errors in the edited file.

Additional quick pass:
- Date: 2025-10-22 (Evening)
- Performed a full scan of `lib/edit/character.txt` for all `C:` lines referencing skill 7 (Song) and corrected remaining mismatches so indices match `lib/edit/ability.txt`:
   - Fixed Finarfin to start with Song of the Trees (7) and Woven Themes (9).
   - Fixed Húrin to include Song of Slaying (10) and Song of Staying (11) in the `C:` list.
   - Fixed Elu Thingol to include Song of Mastery (14) where intended.

All edited files parsed with no errors after the changes.

# Session Notes - Score Display Fix (Final)

## Date
2025-10-19

## Final Implementation

### Short Score List Display - Clean, Polished Layout

**File Modified:** `src/files.c`

**Changes Made:**
1. ✅ Added 1-column right margin for cleaner visual appearance (verdict_width = line_width - 1)
2. ✅ Removed "Order:" prefix from caption (just shows "Score (highest first)" instead)
3. ✅ Smart truncation keeping "at XXft" depth visible
4. ✅ Dynamic terminal width detection

**Layout:**
```
                    Halls of Mandos
Score (highest first)                      Layout: Short

1. Maedhros             777  Escaped with **
2. King Azaghal         673  Escaped with **
3. Fingon               660  Escaped with ***
4. Hador                192  Slain by a Young fire-drake at 800ft
```

**Column Structure:**
- **Place:** 4 chars (`"1. "`)
- **Name:** 15 chars (left-aligned)
- **Score:** 5 chars (right-aligned)
- **Gap:** 2 spaces
- **Verdict:** terminal_width - 26 - 1 (one empty column on right for clarity)

**Smart Truncation:**
- Full verdict always kept if space allows
- If truncating needed, finds " at " (depth marker)
- Truncates monster name but keeps "at XXft" visible
- Example: `"Slain by an... at 100ft"` (depth always shown)

**Indicators:**
- `*` = Silmaril (1-3)
- `V` = Morgoth slain

**Build Status:** ✅ Successful

### 2025-03-19 - Monster runtime stat persistence groundwork
- Bumped VERSION_EXTRA to 3 so the new overrides block loads only on compatible saves.
- Snapshot pristine monster race data into new r_base during init_angband() for baseline comparisons.
- Save pipeline writes per-race runtime stat overrides (stats, blows, flags, visuals) when they diverge from base; loader reapplies them for sf_extra >= 3 else restores base defaults.

- Songs updated: Song of Challenge now applies a small Perception/Stealth penalty and Song of Elbereth shaves Will via monster_skill for on-the-fly reactions.
- Fixed Song debuff build issue by referencing the correct skill_type parameter before re-running CMake build (now clean).
- Added log_debug traces in monster_skill to confirm Song of Challenge/Elbereth penalties at runtime.

## 2025-10-20 - Song of Shattering implementation
- Added new Song of Shattering ability (`SNG_SHATTERING`) with data entry in `lib/edit/ability.txt`, prerequisites, and updated song enumeration/order.
- Extended `monster_type` with persistent shattering fields; save/load path bumped to `VERSION_EXTRA 4` and now serialises damage-side/protection reductions.
- Implemented runtime helpers to honour reduced protection/damage (`monster_base_armour_sides`, melee side clamps) and wired new song logic with distance-scaled Will checks against equipped foes.
- Updated song loop/UI listings so the new song appears in selection and deducts extra voice cost when active.
- Added explicit `HAS_WEAPON/HAS_ARMOUR` monster flags plus data tagging for orcs, trolls, giants, and balrogs so Song of Shattering keys off equipment-bearing foes.
- Physical shattering now chips held gear and nearby floor weapons/armour when damageable, reducing dice toward base values while honouring artefact resistance and visibility messaging.
- Recognise the `INSCRIP_INDESTRUCTIBLE` tag (plus artefact status) when evaluating shatter targets so indestructible gear remains immune.
- Revamped savefile compatibility guard to compare full version tuples (major/minor/patch/extra) and drive feature gates, keeping older saves loadable after future version bumps.

# Session Notes - Blessing Threshold Controls

## Date
2025-10-23

## Blessing Threshold Adjustments

- Expanded runtype data (lib/edit/runtypes.txt, src/types.h, src/init1.c) to support easier/normal/harder blessing thresholds via new `L:` directive values and `blessing_threshold_modes`.
- Introduced per-metarun threshold mode persisted in the first runtime byte (src/metarun.h, src/metarun.c); recalculation logic now pulls the selected mode and falls back gracefully to normal thresholds.
- Added `f` shortcut to the metarun statistics screen with a dedicated menu for selecting easier/normal/harder thresholds, including descriptive guidance and live recalculation/update plus persistence.
- Updated blessing/curse info menu to label curse effects explicitly and surface blessing descriptions/effects for all identified curses (show_known_curses_menu).
- Blessing pool summaries and the blessing exchange dialog now present the active threshold mode while reflecting the selected progression values.


- Updated blessing threshold menu to support arrow-key navigation with in-menu confirmation prompts.
- Active curse/blessing listings (stats and full view) now surface effect text when identified, drawing from P:/H: entries.

# Session Notes - Song of Elveness

## Date
2025-10-22

- Added Song of Elveness to `lib/edit/ability.txt` before Song of Staying with matching prerequisites (`Song of the Trees`), cost, and new description.
- Shifted downstream song IDs (Staying onwards) and refreshed song enumerations (`src/defines.h`), name tables (`src/birth.c`), and song listings (`lib/edit/actual_abilities*.txt`, `lib/edit/character.txt`, `lib/edit/artefact.txt`).
- Implemented gameplay effects: Grace +1 via `calc_bonuses`, Evasion bonus `1 + song/7`, and updated per-turn handling (`src/xtra1.c`, `src/spells1.c`) including noise contribution and UI messaging.

# Session Notes - Song of Disguise

## Date
2025-10-22

- Added Song of Disguise ahead of Song of Lorien in `lib/edit/ability.txt`, keyed to Song of Silence, and propagated the new ordering through song enums, name tables, hero ability maps, and artefact comments.
- Wired `SNG_DISGUISE` behaviour in `src/spells1.c`: enforced start restrictions when observed, tracked pacified/seen-through monsters with per-turn skill contests against Will+Perception (distance, attack, and suspicion penalties), and applied the 2 voice per round upkeep.
- Hooked monster attack tracking and cleanup (`src/melee1.c`, `src/dungeon.c`, `src/monster2.c`) plus AI suppression (`src/melee2.c`) so fooled foes skip their turns until they pierce the disguise; integrated song noise and ability bonus adjustments (`src/xtra1.c`).
- Declared new song helpers in `src/externs.h` and ensured per-turn rotation/reset flows manage disguise state during level transitions and saves.

# Session Notes - Song of Revealing

## Date
2025-10-22

- Added persistent Song of Revealing hints so partially detected monsters render with the listening-style `?` marker by tracking per-monster reveals (`src/spells1.c`) and exposing `song_revealing_overlay`.
- Updated the map renderer (`src/cave.c`) to query the overlay helper so redraws no longer wipe the hint immediately; hints clear automatically when the song stops or monsters are removed.
- Re-ordered Song of Revealing processing to reset hint state each turn while keeping item reveal behaviour intact; linked overlay declaration through `src/externs.h`.
- Introduced a short-lived decay timer for Song of Revealing hints so partial detections persist for several beats even if a later roll fails, avoiding the instant flicker that previously occurred.

# Session Notes - Monster Recall Instance Stats

## Date
2025-10-24

- Threaded an optional `monster_type*` through `screen_roff`, `display_roff`, and `describe_monster`, updating all call sites (`cmd3.c`, `cmd4.c`, `xtra1.c`, `xtra2.c`, `wizard1.c`) so recall views can access live monster state when inspecting a visible target.
- Reworked `describe_monster_movement`, `describe_monster_toughness`, `describe_monster_skills`, and `describe_monster_attack` to pull per-instance data: numeric speed output with hasted/slowed markers, current/max HP ranges with curse/song adjustments, protection ranges reflecting armour penalties/bonuses, skill readouts via `monster_skill`, and blow damage/attack values recalculated with song-induced reductions.
- Swapped legacy `XdY` displays for `min-max` ranges when only race data is available, while defaulting to live `current-max` spans whenever the specific monster is known.

# Session Notes - Recall Dice Formatting

## Date
2025-10-24

- Restored XdY formatting for monster attacks and protection in `src/monster1.c`, keeping adjusted dice from any active debuffs while reverting away from min/max spans.
- Reverted monster HP recall to the base `hdice`/`hside` expression and appended a `-<amount>` suffix when Song of Lament reductions apply, via a new per-monster accumulator backed by `monster_song_hp_loss()`.
- Repurposed the song duel padding bytes (`song_hp_loss_lo/hi`) with save/load support (`src/save.c`, `src/load.c`) and helper accessors (`src/monster2.c`, `src/externs.h`) so song-induced HP penalties persist across turns and savefiles.

# Session Notes - Post-Death Spectator View

## Date
2025-10-25

- Added `death_spectator_view()` (declared in `src/externs.h`, implemented in `src/dungeon.c`) to drive a post-mortem spectator loop that reveals the full dungeon, blocks any command that would spend energy, and allows UI/navigation menus until the player presses `Esc`.
- Guarded `process_command()` with a `death_spectator_mode` whitelist so movement, inventory interaction, and other time-advancing actions are rejected gracefully while the spectator is active.
- Updated `close_game_aux()` (`src/files.c`) to launch the spectator view immediately after scoring and before displaying the tombstone, and wired the tomb menu's "View dungeon" entry to reuse the new spectator loop instead of the old `do_cmd_look()` snapshot.

# Session Notes - Final View Read-Only Polish

## Date
2025-10-25

- Exposed `death_spectator_active()` from `src/dungeon.c` so UI layers can detect the death-view state; inventories, equipment menus, and the supplies browser now call this helper to suppress `use`, `drop`, and other energy-spending actions while still allowing examination flows (`src/object1.c`, `src/cmd3.c`, `src/cmd4.c`).
- Tomb menu no longer offers the inventory/equipment branch, labels the dungeon revisit as “Final look,” and renumbers downstream options accordingly (`src/files.c`).
- Main menu entries for suicide, save, and quit-with-save render disabled and emit a warning if triggered while the corpse view is active, with navigation skipping those slots (`src/cmd4.c`).
## 2025-10-27 - Throwing Mastery & Polearm Updates

- Inserted a new melee ability (Throwing) ahead of Polearm Mastery, bumped downstream IDs/prerequisites, and updated ability tables/hero maps (lib/edit/ability.txt, lib/edit/actual_abilities_*.txt, lib/edit/character.txt, src/defines.h, src/birth.c).
- Added player_can_treat_as_throwing[_flags]() to centralize dynamic throwing checks and declared the helpers in src/externs.h.
- Hooked the new ability into thrown combat: +1 attack, half distance penalty, and Finesse-grade crit separation when hurling items flagged via the helper (src/cmd2.c, src/cmd1.c).
- Extended quiver UI/load paths to honor Polearm Mastery for great spears (slot selection, inventory carry, bonus application) and made thrown breakage/penalties respect the helper (src/cmd3.c, src/object2.c, src/xtra1.c, src/cmd2.c).
- Widened crit_bonus() with an object parameter so throwing mastery can apply selectively, updating all call sites (src/cmd1.c, src/cmd2.c, src/cmd3.c, src/melee1.c, src/spells1.c).

