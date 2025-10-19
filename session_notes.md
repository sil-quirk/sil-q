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
