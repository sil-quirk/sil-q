# Sil-Morë – AI Agent Playbook

## Quick Orientation
- C17 roguelike fork; always include `src/angband.h` so globals (`p_ptr`, `op_ptr`, `z_info`) are in scope.
- Core loop lives in `dungeon.c`; player commands run through `cmd1.c`-`cmd6.c`; inventory/object systems sit in `object1.c`/`object2.c`.
- Game data is text-first under `lib/edit/`; parser changes usually touch `init2.c` and `files.c`.

## Build & Run
- **SDL3 builds (recommended)**: Use `build-cmake.bat` from the repo root. This builds to `build/sil-more.exe` and deploys to `sil-more-windows-sdl3/` with all dependencies.
- Launch SDL3 build with `sil-more-windows-sdl3/sil-more.exe`
- Ensure the Cygwin login shell keeps `/usr/bin:/bin` ahead of `PATH` (VS Code terminal profile already exports this).

## Key Systems (2025-09)
- Combat history menu is production-ready: main menu entry, 100-round circular buffer, full color formatting, search, and scrolling.
- Enhanced inventory/equipment overlays mirror `show_*` logic while adding scrolling, floor item mixing (`-)` labels), and Steam Deck-aware shortcuts.
- Unified look command merges look/locate/object views; Tab cycles entities on the same square, Shift+arrows pan, viewport restores cleanly.
- Combat roll overlay displays 0-3 lines at `COL_MAP`; keep the 65-character clear width unless the layout changes.

## Data, Saves & Logs
- Metarun save backup moves files into timestamped folders like `lib/save/saves_metarun_YYYYMMDD_HHMMSS/`; never revert to the tar-based system.
- **Log file locations**:
  - SDL3 builds: `sil-more-windows-sdl3/log.txt` (deployment directory)
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

## Python Environment & Scripts
- Python 3.13.2 virtual environment at `src/.venv/` with pre-installed dependencies (numpy, pandas, pillow, etc.).
- **Correct executable path**: `C:/Users/efrem/Documents/GitHub/sil-qh/src/.venv/Scripts/python.exe`
- **In PowerShell**, always prefix with `&` operator: `& "./src/.venv/Scripts/python.exe" script.py` (PowerShell treats `-` and `--` as operators).
- When running Python from agents, use the full venv path or activate in terminal. Refer to `PYTHON_SETUP.md` for detailed usage examples.
- Utility scripts: `calc_all_resolutions.py`, `lib/edit/Power_Ratings.py`, `lib/edit/parse_and_align_abilities.py`, `lib/xtra/graf/osx_bmp2png.py`.

## Watchpoints
- UI tweaks often shift highlight alignment or sidebar clears; re-test inventory, equipment, and unified look flows after changes.

## Documentation Protocol
- **Maximum one working notes file per chat session** - Create or update `session_notes.md` in the root directory for technical details, decisions, and progress tracking.
- Keep notes concise and focused on technical specifics: code changes, build issues, configuration updates, and next steps.
- Update the same file throughout the session instead of creating new files.
- Never create verbose multi-file documentation sets during active development; save detailed write-ups for PR descriptions or post-session documentation.

## Reference
- Pair this guide with `AGENTS_MEMORY.md` for deeper implementation notes and troubleshooting checklists.
- See `PYTHON_SETUP.md` for comprehensive Python environment guidance.
- Legacy deep-dive documents remain under `.github/instructions/` for historical context.
