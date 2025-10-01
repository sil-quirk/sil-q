# Sil-QH Agent Memory

This file expands on `AGENTS.md` with implementation detail, troubleshooting cues, and validation checklists drawn from `.github/instructions/`.

## Build & Environment
- Build with `make -f Makefile.cyg -j8`; the target is configured for `-std=c17` and mingw32.
- Use the Cygwin login shell (`C:\Soft\Cygwin\bin\bash.exe --login`) so `/usr/bin:/bin` lead `PATH`; VS Code workspace settings already enforce this.
- Running `make -f Makefile.cyg launch` from the repo root keeps resource lookups (`lib/`) and INI persistence correct.
- Expect `log.txt` to land in the directory you launched the executable from (root vs `src/`).

## Systems Detail

### Combat History Menu
- Accessed from the main menu between Log and Options (`combat history (x)`).
- Stores up to 100 rounds in `combat_history[MAX_COMBAT_HISTORY]`; `add_combat_round_to_history()` is hooked from `new_combat_round()`.
- Viewer `do_cmd_combat_history()` mirrors combat roll formatting (color, alignment) and supports `p`/`n` scrolling, `+`/`-` page jumps, `/` search, `=` highlight, `4`/`6` horizontal scroll.
- Preserve the 65-character clear width (`COL_MAP` anchored) when altering combat overlays.

### Enhanced Inventory & Equipment
- Overlays replicate `show_inven()` / `show_equip()` output by reusing their column and row calculations; highlight rows track the overlay index, not slot numbers.
- No `Term_clear()` calls remain; screen transitions rely on `screen_save()`/`screen_load()`; keep it that way to avoid black screens.
- Floor items display with the `-)` prefix and respect the `-` shortcut; equipment navigation mirrors classic commands plus overlay cycling (`u`/`x`).

### Unified Look & Scrolling
- `do_cmd_unified_look()` governs both sidebar and square cycling; `unified_look_state` includes `current_square_entity` and `square_cycling_mode` (see `src/defines.h`).
- Tab cycles entities on the current square when both monster and object exist; `q` or `` ` `` provide backward cycling, and arrow movement exits square-cycling mode.
- Banner clearing is immediate: when `g_banner_force_redraw_remaining > 0`, commands call `do_cmd_redraw()` after zeroing the counter (`cmd3.c` / `cmd4.c`).
- Sidebar rendering (`show_unified_sidebar()`) updates the map cursor to match the selected entity and highlights pictograms without visibility gating.

### Combat Roll Overlay
- Optional overlay writes up to three lines starting at `COL_MAP`; lines auto-trim with a fixed 65-character clear width.
- Overlay stays dormant until combat occurs; maintain lazy activation when modifying combat flow.

### Windowing & Fullscreen
- `set_subwindow_fullscreen_style()`, `enter_fullscreen()`, and `exit_fullscreen()` guard all `SetWindowLong` calls with error checks (`GetLastError()`), style validation, and `SWP_FRAMECHANGED` updates.
- Normal, borderless, and minimal window styles are validated before and after fullscreen; keep Z-order corrections intact to avoid hidden subwindows.
- Logging uses `log_error()`/`log_debug()` for transition failures; do not remove these diagnostics.

### Metarun Save Pipeline
- `backup_and_clear_saves()` moves files into timestamped directories like `lib/save/saves_metarun_YYYYMMDD_HHMMSS/` (uses `_mkdir`/`mkdir` + `rename`).
- `.gitignore` and existing backup folders are preserved; avoid reintroducing tar/zip archives or copy-based backups.

### Quest Attribution
- Level markers remain only for location quests (`niena_level`, `mandos_level`, `aule_level`); non-location quests rely on quest state (`QUEST_REWARDED`) without level discriminators.
- `metarun_is_quest_completed()` + quest state determine "previous run" vs "this character" attribution; updates live in `src/xtra2.c` and `src/cmd2.c`.
- Leaving a quest level prematurely now warns the player for location-specific quests; keep the warning hooks when changing quest flow.

## Validation Checklists
- **UI overlays**: After changes, test inventory/equipment toggle, floor pickup/drop, item descriptions, and Steam Deck bindings (`Space` confirm).
- **Unified look**: Validate Tab cycling with mixed monster/object squares, sidebar-only tiles, bigtile mode, and banner clearing.
- **Windowing**: Run fullscreen toggles across all subwindow styles, resizing, and restoration to confirm styles persist and errors log cleanly.
- **Metarun**: Finish a run to confirm save folders move correctly and fresh startups clean the active `lib/save/` directory.

## Diagnostics & Tooling
- Prefer `log_trace()` for temporary instrumentation; player messaging belongs in `msg_print()`/`msg_format()`.
- Screen overlays must wrap rendering with `screen_save()`/`screen_load()` and defer redraws until after restoring the screen.
- Reset `item_tester_*` hooks and other global testers after prompts to avoid state leaks in subsequent commands.

## Reference Map
- `AGENTS.md`: high-level orientation.
- `.github/instructions/memory.instruction.md`: quick historical snapshot.
- `.github/instructions/memory.instruction.detailed.md`: exhaustive change log for 2025 UI/window work.
- `.github/instructions/copilot-instructions.md`: coding conventions and subsystem overview.
