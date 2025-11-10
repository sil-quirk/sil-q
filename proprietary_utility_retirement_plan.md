# Proprietary Utility Retirement Plan

This document inventories the legacy helper layers (primarily `z-*.c` and `util.c`) and lays out a stepwise refactor path that replaces them with modern C17 or SDL3 facilities. Each phase is scoped so the tree keeps building and running (`build-cmake.bat` on Windows / `cmake --build build` elsewhere) before moving on.

## Status Snapshot (2025-11-10)
| Phase | Focus | Status | Evidence |
| --- | --- | --- | --- |
| 0 | Baseline + guardrails | Done | Baseline build + warning count recorded in `session_notes.md:37-124`. |
| 1 | String and memory helpers | Done | `streq/prefix/suffix` now inline in `src/angband.h:94-110`; `z-virt.c` uses SDL allocators; logged in `session_notes.md:39-134`. |
| 2 | SDL-backed file/path utilities | Done | `sdl_fopen` and friends live in `src/util.c:299-629` and are used throughout loaders/dumps (`src/cmd4.c:170-260`, `src/dump_items.c:80-949`). |
| 3 | Formatting + logging glue | Done (cleanup pending) | `src/format.c`/`src/format.h` own the public helpers (`session_notes.md:4235-4277`); `z-form.c` now only houses the legacy `strnfmt` core. |
| 4 | RNG + math helpers | Done (compatibility baseline) | `src/rng.c`/`src/rng.h` replace `z-rand.*` (`session_notes.md:3-84`) while preserving deterministic behavior. |
| 4b | SDL random context integration | Planned | Migrate RNG internals onto `SDL_CreateRandomContext` without changing the public API. |
| 5 | Terminal abstraction | Planned | Collapse `z-term` once SDL panes cover all rendering/input needs. |
| 6 | Final deletion | Planned | Remove the remaining `z-*` files and normalize headers/docs. |

## Goals
- Retire bespoke portability shims (string/memory helpers, RNG, term package, path/file wrappers) that date back to pre-SDL ports.
- Lean on SDL3 (windowing, input, timing, RNG, file I/O helpers) or the C17 standard library to simplify maintenance.
- Delete dead platform code; SDL3 is now the only supported frontend (`session_notes.md:4115-4138`), so we can excise residual guards instead of keeping compatibility abstractions.
- Adopt modern C17 practices (explicit sizing, `bool`, `size_t`, `const`, error propagation) as we touch each subsystem.
- Restructure files so functionality lives in focused modules rather than monolithic legacy buckets.

## Codebase Snapshot (2025-11)
- `src/` — gameplay core (`cmd*.c`, `object*.c`, `dungeon.c`, etc.), SDL-only frontend (`main-sdl.c`), and the legacy utility layer (`z-*.c`, `util.c`, `files.c`). The old `main-gcu.c` / `main-win.c` stubs have been deleted.
- `lib/` — text data (`lib/edit/*.txt`), prefs (`lib/pref/*.prf`), and fonts/palettes (`lib/pref/font-*.prf`).
- `sil-more-windows-sdl3/` — deployment payload (game assets + `log.txt` runtime output).
- Docs/scripts — migration notes (`SDL_CLEANUP_PLAN.md`, `SDL_MIGRATION.md`), current-session log (`session_notes.md`), and helper scripts under `tools/` or repo root.

Despite the SDL-only pivot, the gameplay loop still routes through the Angband-era `term` abstraction, bespoke random/formatting helpers, and a monolithic `util.c`. Those are the remaining seams we are collapsing.

## Legacy Utility Inventory
| Module | Responsibilities today | Replacement direction |
| --- | --- | --- |
| `z-util.c/h` | Residual case-insensitive compare helpers (`z-util.c:24-119`) plus historical logging hooks. | Delete the unused functions and move any needed declarations into modern headers; rely on SDL/standard logging plus `log/log.h`. |
| `z-virt.c/h` | Allocation macros (`C_MAKE`, `KILL`, etc.) and wrappers around SDL memory calls. | Replace macros with explicit helpers or typed inline functions; migrate remaining call sites to SDL allocators so the file can be deleted. |
| `z-form.c/h` | Legacy `strnfmt` implementation, static format buffer (`z-form.c:600-642`), and helper glue. | Move `strnfmt/vstrnfmt/strnfcat` into `src/format.c`, drop the static buffer, then delete `z-form.*`. |
| `z-rand.c/h` | Replaced by `src/rng.c/rng.h`; still uses bespoke LCRNG arrays internally. | Finish Phase 4b by backing RNG state with `SDL_RandomContext` instances while keeping deterministic seeds. |
| `z-term.c/h` | Terminal/window abstraction predating SDL panes. | Route rendering/input through SDL view helpers and delete the terminal layer. |
| `util.c` | Path parsing (`path_parse` at `src/util.c:129-378`), filesystem helpers, logging init, color utilities, and miscellaneous shims. | Split into focused modules (filesystem, logging bootstrap, color helpers, birth helpers) so the last legacy pieces can be deleted alongside `z-*`. |

## Active Migration Roadmap
Each phase should end with a clean SDL3 build and a smoke run (new game -> spend a few turns -> quit). Later phases also require targeted playtest scenarios (combat history, story font overlays, etc.).

### Phase 3 - Replace Formatting + Logging Glue (`z-form`, `plog/quit/core`)
**Status:** Done (cleanup pending)  
**Scope:** Consolidate on SDL/C17 formatting while centralizing logging/quit hooks.
- Added `src/format.c`/`src/format.h` with `format()`, append helpers, and logging-safe wrappers so callers stop depending on `z-form.h`.
- Remaining work: move `strnfmt/vstrnfmt/strnfcat` into `format.c`, delete the static format buffer in `z-form.c:600-642`, and remove legacy `plog_fmt/quit_fmt/core_fmt` once everything uses `log/log.h`.

### Phase 4 - RNG + Math Modernization (`rng.c`, probability helpers)
**Status:** Done (compatibility baseline)  
**Scope:** Provide a modern home for RNG APIs without altering gameplay outcomes.
- Created `src/rng.c`/`src/rng.h` and updated all includes to use the new module; `z-rand.*` has been deleted.
- Gameplay continues to use the same deterministic algorithms, so saves remain compatible while we prepare to adopt SDL’s RNG internals in Phase 4b.

### Phase 4b - SDL Random Context Integration (next focus)
**Status:** Planned  
**Scope:** Move RNG internals from bespoke LCRNG arrays to SDL primitives.
- Introduce one or more `SDL_RandomContext` instances (`rng_main`, `rng_quick`, `rng_ui`) seeded from the same values we persist today.
- Re-implement `Rand_div`, `Rand_normal`, `one_in_`, etc., on top of SDL’s API while keeping the public signatures intact (callers still include `rng.h`).
- Add migration tests: capture the RNG state before/after scripted sequences, run statistical comparison scripts, and ensure save/load determinism before flipping the switch.
- Current progress: the RNG now uses a single SDL-backed state (`Rand_state_export` / `Rand_state_import`), and callers that need deterministic sub-sequences save/restore that state instead of toggling a separate `Rand_simple()` path.

### Phase 5 - Collapse `z-term` in Favor of SDL Views
**Status:** Planned  
**Scope:** Remove the Angband terminal abstraction once SDL panes cover every feature.
- Catalogue every remaining `Term_*` usage and map it to an SDL pane responsibility (message log, combat rolls overlay, unified look UI, etc.).
- Add SDL-native helpers (render queues, cursor draw, input dispatch) so gameplay code talks directly to SDL instead of the legacy terminal struct.
- Once gameplay code calls SDL helpers, delete the unused `term` flags, screen images, and queueing logic in `z-term.c` and shrink `term` to an internal SDL view struct.
- Regression-test UI-heavy flows: combat history, inventory/equipment overlays, unified look, Steam Deck shortcuts, auto list drop-downs.

### Phase 6 - Final Cleanup & z-* Deletion
**Status:** Planned  
**Scope:** Remove retired modules and normalize the tree.
- Delete remaining `z-*.c/.h` once their responsibilities are absorbed; stop including them from `angband.h` and `externs.h`.
- Break up `util.c` into focused modules (filesystem, logger bootstrap, color tables, birth helpers) so any stragglers from earlier phases move into obvious homes.
- Update docs (`README.md`, `SDL_MIGRATION*.md`, `session_notes.md`) and build files to reflect the final module layout; ensure no stale references remain.
- Finish with a clean build/test sweep and (ideally) a reduced warning count thanks to the slimmer abstraction layer.

## File Restructuring & z-* Retirement
1. **Strings and memory (cleanup pending):** Inline helpers live in `src/angband.h:94-110`, but `z-util.c` still contains dead `my_str*` definitions. Remove them once all callers include `format.h`/`angband.h` instead of `z-util.h`.
2. **Filesystem breakout:** Move the SDL IO helpers from `src/util.c:299-629` into `src/fs/io_sdl.c` + `fs/io_sdl.h` so consumers (`cmd4.c`, `dump_items.c`, `save.c`, etc.) no longer need the entire `util.c` surface. Replace `path_parse/path_temp` with SDL path helpers to drop 1990s-era tilde/drive parsing rules.
3. **Logging/bootstrap module:** Extract `init_logger()` and related helpers from the bottom of `util.c` into `src/logging/bootstrap.c`, leaving only prototypes in `externs.h` and making teardown flows (`atexit`, `SDL_Quit`) consistent.
4. **Color and text helpers:** Relocate `short_color_names`, `attr_to_text`, and related UI tables into `src/ui/colors.c`. This isolates UI data from `util.c` and primes us to delete `z-term` once panes own the palette logic.
5. **Formatting cleanup:** Finish migrating `strnfmt`/`vstrnfmt`/`strnfcat` into `format.c`, then delete the redundant code in `z-form.c`. Update callers to include `format.h` so `z-form.h` can be retired.
6. **RNG cleanup:** With the single-state SDL RNG in place, finish migrating any callers that still expect legacy globals, document the 64-bit serialization format, and add helper APIs for deterministic push/pop usage so gameplay code stops open-coding seed swaps.
7. **Terminal abstraction:** Create `src/ui/term_sdl.c` to house any remaining terminal-like behaviors, then remove `z-term.c/h` once SDL panes cover the rendering/input lifecycle.
8. **Header hygiene:** Stop including `z-*.h` from `angband.h` once functionality migrates. Replace macros (`C_MAKE`, `KILL`, etc.) with typed inline helpers or enums to leverage C17 diagnostics.

## C17 Modernization Checklist
- **`z-util.c:24-119`** — Delete the unused `my_stricmp/my_strnicmp` implementations and strip their prototypes from `z-util.h`.
- **`z-form.c:600-642`** — Remove the static format buffer once `format.c` owns `strnfmt`; replace manual `C_MAKE/KILL` usage with stack buffers.
- **`z-virt.h:32-86`** — Replace macro-heavy allocation helpers with typed inline wrappers that return `bool` or `size_t` for clearer error handling.
- **`src/rng.c`** — Replace manual globals with `SDL_RandomContext` instances (Phase 4b) and encapsulate state in structs to simplify serialization.
- **`z-term.c`** — Continue carving UI helpers into SDL-aware modules so the Angband terminal abstraction can be deleted.
- **`src/util.c:129-742`** — Replace `path_parse/path_temp` with SDL path helpers; drop historical tilde and drive parsing logic.
- **Global logging/quit hooks** — Collapse duplicate `plog/quit/core` implementations into a single `log_fatal()` path that flushes via `log/log.h` and calls `SDL_Quit`.

## Coordination Notes
- Keep `session_notes.md` updated with phase status, blockers, and verification evidence (per AGENTS guide).
- Stage work so gameplay-critical files (`dungeon.c`, `cmd*.c`) only see mechanical changes in early phases; defer functional simplifications until after the utility surface is gone.
- Reuse the existing logging infrastructure (`log/log.h`) for diagnostics instead of ad-hoc `printf`s while refactoring.

Following this plan moves us from decades-old portability shims to a lean SDL3/C17 codebase without destabilizing core gameplay between steps.
