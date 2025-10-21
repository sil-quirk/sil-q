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
