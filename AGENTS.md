# Sil-QH Agent Guide

High-signal repo guidance for coding agents (Codex CLI, Copilot, etc.). This is the "what matters / where to look / what not to break" file.

## Subagent Policy
- Subagents are allowed when they materially help the task.
- Choose the subagent model and reasoning level yourself based on task difficulty.
- Prefer `gpt-5.6 sol or luna` or `gpt-5.4-mini` for subagents unless a different choice is clearly better for the work.
- When you use a subagent, report which model and reasoning level you chose and why.

## Project Snapshot
- Language: C17 (see `CMakeLists.txt`).
- Primary frontend: SDL3 (`src/main-sdl.c`).
- Data model: text templates in `lib/edit/` compiled into `ANGBAND_DIR_DATA` at runtime (standard: `<user-root>/data/`, portable: `lib/data/`).
- Fork-specific pillars: metaruns + scoring DB (`src/metarun.c`, `src/score/`), drop system (`src/drop_system.c`), SDL config (`sil_sdl.json`) + sound config (`sound.json`).

## Repo Map (Where Things Live)
- `src/`: engine + frontend
  - Core loop / dungeon: `src/dungeon.c`, generation: `src/generate.c`, map: `src/cave/`
  - Commands (player input -> actions): `src/cmd1.c` ... `src/cmd6.c`
  - Monsters/combat/spells: `src/monster1.c`, `src/monster2.c`, `src/melee1.c`, `src/melee2.c`, `src/spells1.c`, `src/spells2.c`
  - Objects/inventory: `src/object/` (`object-desc.c`, `object-inventory.c`, `object-make.c`, `object-ui-*.c`, etc.)
  - Save/load + file/path init: `src/save.c`, `src/load.c`, `src/init2.c`, `src/files.c`
  - UI plumbing: `src/z-term.c` (key queue/Term), `src/pane.c` (subwindows), `src/format.c`, `src/ui/`
  - Metarun + scoring: `src/metarun.c`, `src/metarun_legacy.c`, `src/score/`
  - Logging: `src/log/` (use `log_debug`, `log_trace`, `log_warn`, `log_error`)
  - New `.c` files should start with `#include "angband.h"` (file: `src/angband.h`) so globals like `p_ptr`, `op_ptr`, `z_info` are in scope.
- `lib/`: runtime data + assets
  - Templates: `lib/edit/*.txt` (monsters/vaults/objects/etc)
  - Generated binaries: `*.raw` under `ANGBAND_DIR_DATA` (do not edit; normally not committed)
  - Preferences: JSON defaults in `lib/pref/` (`sound.json`, `palette_presets.json`, object text colors); legacy `.prf` files are removed
  - Assets: `lib/xtra/` (graf/font/music/sound)
- `tools/`: one-off developer utilities
  - Stable GUID generation for templates: `tools/make_guid.py`
- `scripts/`: Python analysis/simulation helpers (many write to `scripts/output/`)
- `docs/`: design notes: `docs/key_handling_report.md`, `docs/score_system_overhaul.md`

## Build & Run (Windows SDL3 - Recommended)
Prereqs: MSYS2 MinGW64 at `C:\msys64` with `cmake` + SDL3 deps installed (the build script assumes this path).

- Build (recommended; use this first): `.\build-cmake.bat`
  - This is the "known good" Windows/SDL3 path for this repo (sets up the toolchain, configures CMake, and stages runtime `lib/` data correctly).
  - Avoid trying to build “directly with MinGW” (`gcc`, `mingw32-make`, ad-hoc include/library paths) unless you are explicitly debugging the build system; it’s easy to end up with the wrong generator/env and waste time.
  - Standard (per-user data): `build-standard/` -> `sil-more-windows-sdl3/`
  - Portable (local data under `lib/`): `build-portable/` -> `sil-more-windows-sdl3-portable/`
- Incremental rebuild from PowerShell/cmd: `.\build-incremental.ps1`
- Incremental rebuild from an MSYS2 MinGW64 shell or any shell that already has `C:\msys64\mingw64\bin;C:\msys64\usr\bin` on `PATH`: `cmake --build build-standard --parallel`
- Do not invoke `C:\msys64\mingw64\bin\cmake.exe --build ...` from an arbitrary shell without first seeding those MSYS2 paths; the build can fail with a generic `Error 1` when GCC helper executables cannot load their DLLs.
- Run:
  - Standard: `.\sil-more-windows-sdl3\sil-more.exe`
  - Portable: `.\sil-more-windows-sdl3-portable\sil-more.exe`

Note: `build-cmake.bat` always refreshes `lib/edit/`, `lib/pref/`, `lib/xtra/sound/`, and `lib/xtra/graf/16x16.png` in deployments. If you change other `lib/` assets (help/music/fonts/etc), delete the deployment `lib/` folder or update the script so the change actually ships.

## Runtime Paths (Standard vs Portable)
Path resolution is centralized in `src/init2.c:init_file_paths()`.

- Standard build (`SIL_USE_LOCAL_DATA=OFF`):
  - Uses `SDL_GetPrefPath("Sil-QH", "sil-more")` as the user root.
  - Saves: `<user-root>/save/`
  - User config: `<user-root>/sil_sdl.json` and `<user-root>/sound.json` (seeded from `lib/pref/sound.json` if missing)
  - Metaruns: `<user-root>/meta/metaruns/`
  - Scores + DBs: `<user-root>/meta/` (legacy `scores.raw` plus `runs.db` and related DBs)
- Portable build (`SIL_USE_LOCAL_DATA=ON`):
  - Uses folders under the deployed `lib/` directory: `lib/save/`, `lib/user/`, `lib/data/`, `lib/apex/`, `lib/apex/metaruns/`
  - Sound config reads from `lib/pref/sound.json` (see `src/main-sdl.c`).

## Data Workflow (`lib/edit/*.txt`)
These templates drive gameplay content (monsters, vaults, objects, terrain, quests, styles, etc.). They compile into `ANGBAND_DIR_DATA` (`<user-root>/data/` or `lib/data/` depending on build).

- Common templates:
  - Monsters: `lib/edit/monster.txt`
  - Vaults/rooms: `lib/edit/vault.txt`
  - Objects/artefacts: `lib/edit/object.txt`, `lib/edit/artefact.txt`
  - Terrain: `lib/edit/terrain.txt`
- Keep numeric IDs stable:
  - Most templates use `N:<serial>:` entries; **serials must strictly increase**. Append new entries; avoid renumbering/reordering existing ones (can break save compatibility and analytics).
- Maintain stable GUIDs:
  - Many templates support `Q:<hex-guid>` per entry. These GUIDs are used by the score/analytics system and must never change once shipped.
  - Add missing GUIDs with: `python tools/make_guid.py` (or pass specific files). Use `--dry-run` to preview.
- Forcing regeneration:
  - If you need to force a rebuild after template changes, delete the relevant `*.raw` in the active data folder (`<user-root>/data/` for standard builds, `lib/data/` for portable builds) and rerun.

## Key Subsystems (Fast Pointers)
- Key handling pipeline (SDL -> command): see `docs/key_handling_report.md` and `src/z-term.c`, `src/util.c:inkey()`/`request_command()`.
- SDL UI configuration: `src/sdl-config.c` reads/writes `sil_sdl.json` (pane layout, scaling, fullscreen, etc.).
- Sound system: `src/sdl-sound.c` + `src/sound-config.c` read `sound.json` and map events (see `angband_sound_name[]` in `src/variable.c`).
- Metaruns: `src/metarun.c` and metarun cleanup in `src/files.c`.
- Score/DB layer: `src/score/` (see `docs/score_system_overhaul.md`).
- Drop system: `src/drop_system.c` caches to `ANGBAND_DIR_DATA/drops.raw` and regenerates when relevant edit files change.
  - **Smithing Difficulty Sync**: Changes to the difficulty calculation algorithm in `src/drop_system.c` MUST be synchronized with `scripts/calc_artefact_difficulty.py`. Both files implement the same `object_difficulty()` logic. When updating penalty flags, bonuses, or multipliers, update both files identically to keep the analysis tool in sync with the engine.
- Combat history viewer: `src/melee1.c:do_cmd_combat_history()` (hooked from `src/cmd4.c`).
- Unified look: `src/cmd3.c:do_cmd_unified_look()` and sidebar in `src/cmd4.c:show_unified_sidebar()`.

## Coding Patterns (C)
- Includes: start new `.c` files with `#include "angband.h"`.
- New `.c` files must be added to the explicit list in `CMakeLists.txt`.
- Cross-file globals: define in `src/variable.c`, declare in `src/externs.h` (prefer not adding new globals).
- Strings: prefer `my_strcpy`, `strnfmt`, `SDL_strlcpy`; avoid `strcpy`, `sprintf`.
- Linkage: prefer `static` helpers within a translation unit; only promote to `externs.h` when necessary.
- UI redraw contract: set `p_ptr->redraw`/`p_ptr->window` bits, then let `handle_stuff()` repaint after `screen_load()`.
- State hygiene: reset `item_tester_*` and similar selection globals immediately after use.

## UX & Gameplay Guardrails (Do Not Regress)
- Inventory/equipment overlays: reuse `show_inven()` / `show_equip()` layout math; do not hard-code columns/offsets.
- Floor items: must keep the `-)` prefix and respond to the `-` shortcut.
- Combat roll overlay: anchored at `COL_MAP`, max 0-3 lines by default; preserve the 65-character clear width unless the layout changes (`SCREEN_HGT` depends on `op_ptr->main_combat_rolls`).
- Screen overlays: prefer `screen_save()` / `screen_load()`; avoid `Term_clear()`-based hacks that cause black screens or highlight drift.
- Steam Deck: with `STEAMDECK_SUPPORT`, treat Space as confirm for stairs/shafts; otherwise keep explicit `y/n` prompts.
- Preserve auto list drop-downs (`OPT_auto_display_lists`) and current menu cycling behavior (`u`/`x`).

## Save/Format Changes (When You Add Persistent State)
- Versioning is centralized in `src/defines.h` (`VERSION_*`, `MIN_VERSION_EXTRA`). Bump compat version when save/scoring/metarun formats change.
- When adding fields to persistent structs:
  - Update writers (`src/save.c`) and readers (`src/load.c`) and gate new reads via `savefile_version_at_least()`.
  - Add defaults for older saves and keep failure modes safe (log + recover where possible).

## Diagnostics
- Use logging (`log_trace`/`log_debug`/`log_info`/`log_warn`/`log_error`) instead of ad-hoc prints.
- `log.txt` is created next to the launched executable; always check the folder you ran from.
- For path issues, `src/init2.c` logs the resolved `ANGBAND_DIR_*` paths at startup.

## Quick Validation
- Build: `.\build-cmake.bat` (or incremental `.\build-incremental.ps1`).
- Run the relevant exe and check the adjacent `log.txt` for errors.
- If UI changed, smoke-test inventory/equipment overlays, unified look, and the combat roll overlay.

## Generated Artifacts (Don't Edit/Commit)
- Build outputs: `build*/`, `sil-more-windows-sdl3*/`, `*.o`, `*.raw`, `log.txt`, and release `*.zip` are generated.

## Python & Tools
- Venv: `src/.venv/` (PowerShell example: `& "./src/.venv/Scripts/python.exe" tools/make_guid.py --dry-run`).
- Analysis scripts: `scripts/*.py` (many emit CSV/plots under `scripts/output/`, which is ignored).

## Release/Packaging Helpers (Optional)
- Build folders for distribution: `create-release-build.ps1`
- Zip a release folder: `create-distribution-archive.ps1`

## Documentation Protocol
- **Maximum one working notes file per chat session**: use `session_notes.md` in the repo root.
- Keep notes technical and concise (what changed, where, why, how to validate).

## References
- `AGENTS_MEMORY.md`: deeper implementation notes/checklists (some historical notes may be stale).
- `.github/instructions/`: legacy agent guidance and historical context.
