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
