# Sil-QH Agent Guide

## Quick Orientation
- C17 roguelike fork; always include `src/angband.h` so globals (`p_ptr`, `op_ptr`, `z_info`) are in scope.
- Core loop lives in `dungeon.c`; player commands run through `cmd1.c`-`cmd6.c`; inventory/object systems sit in `object1.c`/`object2.c`.
- Game data is text-first under `lib/edit/`; parser changes usually touch `init2.c` and `files.c`.

## Build & Run
- **SDL3 builds (recommended)**: Use `build-cmake.bat` from the repo root. This builds to `build/sil-more.exe` and deploys to `sil-more-windows-sdl3/` with all dependencies.
- **Legacy Win32 builds**: Run from `src/` using `C:\\Soft\\Cygwin\\bin\\bash.exe -lc "cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src && make -f Makefile.cyg -j8"` (VS Code's external terminal already launches this shell).
- Launch SDL3 build with `sil-more-windows-sdl3/run.bat`; Win32 build with `make -f Makefile.cyg launch` from `src/`.
- Executables expect to start in the repo root so `lib/` lookups succeed.
- Ensure the Cygwin login shell keeps `/usr/bin:/bin` ahead of `PATH` (VS Code terminal profile already exports this).

## Key Systems (2025-09)
- Combat history menu is production-ready: main menu entry, 100-round circular buffer, full color formatting, search, and scrolling.
- Enhanced inventory/equipment overlays mirror `show_*` logic while adding scrolling, floor item mixing (`-)` labels), and Steam Deck-aware shortcuts.
- Unified look command merges look/locate/object views; Tab cycles entities on the same square, Shift+arrows pan, viewport restores cleanly.
- Combat roll overlay displays 0-3 lines at `COL_MAP`; keep the 65-character clear width unless the layout changes.

## Windowing & Fullscreen
- Fullscreen and subwindow style transitions now guard every `SetWindowLong` call, validate saved styles, and maintain Z-order; use the existing helpers instead of touching WinAPI calls directly.
- Error handling logs via `log_error()` when fullscreen transitions fail; leave the checks intact when refactoring.

## Data, Saves & Logs
- Metarun save backup moves files into timestamped folders like `lib/save/saves_metarun_YYYYMMDD_HHMMSS/`; never revert to the tar-based system.
- **Log file locations**:
  - SDL3 builds: `sil-more-windows-sdl3/log.txt` (deployment directory)
  - Win32 builds: `log.txt` in repo root or `src/` depending on launch location
  - Always check the appropriate location when debugging.

## UX & Gameplay Guardrails
- Do not hard-code offsets: reuse `show_inven()` and `show_equip()` calculations for columns and highlights.
- Floor items must retain the `-)` prefix and respond to the `-` shortcut.
- With `STEAMDECK_SUPPORT`, treat Space as confirm for stairs/shafts; otherwise stick to explicit `y/n` prompts.
- Keep auto list drop-downs (`OPT_auto_display_lists`) enabled by default and preserve current menu cycling behavior (`u`/`x`).

## Coding Patterns
- Follow the redraw contract: set `p_ptr->redraw`/`p_ptr->window` bits, then let `handle_stuff()` drive painting after `screen_load()`.
- Prefer `static` helpers within a TU; expose declarations through `externs.h` only when cross-file access is required.
- Use safe string helpers (`my_strcpy`, `strnfmt`) and the project typedefs for width-stable data.
- Reset `item_tester_*` and other selection globals immediately after use.

## Diagnostics & Tooling
- Use `log_trace`/`log_debug` in place of ad-hoc prints; keep player-facing text in `msg_print`/`msg_format`.
- Development environment expects C17 (`-std=c17`) and the mingw32 toolchain shipped with the repo; no external package manager needed.

## Watchpoints
- UI tweaks often shift highlight alignment or sidebar clears; re-test inventory, equipment, and unified look flows after changes.
- Fullscreen/subwindow work can regress style persistence; exercise the normal, borderless, and minimal paths when modifying window code.

## Reference
- Pair this guide with `AGENTS_MEMORY.md` for deeper implementation notes and troubleshooting checklists.
- Legacy deep-dive documents remain under `.github/instructions/` (memory + copilot guides) for historical context.

