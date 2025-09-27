# Sil-Morë – AI Agent Playbook

## Quick orientation
- C17 roguelike; include `src/angband.h` everywhere and respect the global state `p_ptr`, `op_ptr`, `z_info`.
- Core flow: `dungeon.c` drives turns, `cmd1.c`–`cmd6.c` implement commands, `object1.c`/`object2.c` handle inventory/equipment and object logic, `xtra*.c` host utilities/quests.
- Game data lives under `lib/` (`lib/edit/*.txt` tables, `lib/apex` metarun saves). Changes there usually require matching parser tweaks in `init2.c` or `files.c`.

## Build & run workflow
- From Windows/Cygwin shell, build inside `src/` with `make -f ../Makefiles/Makefile.cyg -j8` (toolchain passes `-std=c17`). Running it from the repo root fails because `sil.rc` is resolved relative to `src`.
- Other platforms retain legacy makefiles (`Makefile.std`, `Makefile.osx`, etc.), but the Cygwin build is the actively maintained path.
- Executables expect to be launched from the repository root so that relative `lib/` lookups succeed.

## UI & menu architecture
- Inventory/equipment menus use enhanced overlays in `object1.c` (`show_inven_enhanced`, `show_equip_enhanced`) which mirror the classic `show_*` routines but add scrolling, floor-item mixing, and highlight state.
- `cmd3.c` wraps those screens via `do_cmd_inven` / `do_cmd_equip`. New global flags (`enhanced_menu_action`, `enhanced_drop_refresh_pending`, etc.) communicate between the overlay and command handlers.
- When mutating state inside a saved screen, set redraw bits and defer `handle_stuff()` + `Term_fresh()` until after `screen_load()`; see how left-arrow drops now raise `PR_MAP`/`PW_MESSAGE` before the final refresh.

## Input, selection & prompts
- `get_item()` lives in `object1.c` and is steered by globals `item_tester_tval`, `item_tester_hook`, and `item_tester_full`; reset them immediately after use. Ring/arrow replacement showcases slot-specific hooks in `cmd3.c`.
- Quantity prompts run through `get_quantity()` in `util.c`, which now supports arrow/page keys and wraps numeric entry (check the interactive loop around line ~3720).

## Game data & quests
- Quest and metarun state is documented in `.github/instructions/memory.instruction.md`; code touches usually span `defines.h`, `types.h`, `save.c`, `load.c`, and quest handlers in `xtra2.c`/`generate.c`.
- Text assets in `lib/edit/` feed the parsers in `init2.c`. Respect existing token formats (`N:`, `G:`, `F:`) when extending tables.

## Rendering & redraw contract
- Use `p_ptr->redraw` (bitmask of `PR_*`) and `p_ptr->window` (`PW_*`) to request UI updates. Actual painting happens in `handle_stuff()` ⇒ `redraw_stuff()` ⇒ `window_stuff()`.
- Screen overlays must bracket drawing with `screen_save()` / `screen_load()`. Avoid calling `Term_clear()` while overlays are active; rely on `prt()`/`c_put_str()` like the enhanced menus do.

## Modern C practices
- Prefer `static` helpers inside translation units and only surface declarations through `externs.h` when they must be cross-file.
- Use project typedefs (`s16b`, `byte`, etc.) or `<stdint.h>` widths when interacting with save files or binary data—keep representation sizes explicit.
- Guard pointer use and array indexing; every floor/inventory traversal already tests `k_idx`/`cave_*`—mirror that pattern for new loops.
- Clip string work with the existing safe wrappers (`my_strcpy`, `strnfmt`) instead of raw `strcpy`/`sprintf`.
- When introducing new globals or flags, initialize them in the appropriate lifecycle spot (`init1.c`, `birth.c`, or right before first use) and clear them after `screen_load()` to avoid leaking state between commands.

## Logging & diagnostics
- `log_trace`, `log_debug`, and `log_error` (see `log.c`) are available and already sprinkled through the enhanced UI. Keep player-facing text in `msg_print` / `msg_format`.
- For temporary debugging, prefer `log_trace()` with clear prefixes (the debug log collects per-run traces in `lib/log.txt`).

## Common pitfalls
- Object indices: inventory uses non-negative indexes, floor objects are stored as negative values (`0 - o_idx`). Always gate floor access with `cave_o_idx[y][x]` checks.
- Save compatibility: new `player_type` fields require matching read/write blocks in `save.c`/`load.c` and initialization in `birth.c`.
- Menu-state leaks: after restricting selections with `item_tester_*`, ensure they’re cleared before returning. Forgetting this can lock later prompts to the wrong tvals.
- Build quirks: Visual Studio projects exist in `msvc2022/` but are experimental; the authoritative compiler path is the makefile build.

## Reference docs worth reading
- `.github/instructions/memory.instruction.md` – rolling knowledge base with recent gameplay/UI changes.
- `FULLSCREEN_*` and other reports in `.github/instructions/` document prior fixes; review before touching those subsystems.
- Root `README.md` captures platform build notes and release roadmap.
