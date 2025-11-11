# Proprietary Utility Retirement Plan

This document inventories the legacy helper layers (primarily `z-*.c` and `util.c`) and lays out a stepwise refactor path that replaces them with modern C17 or SDL3 facilities. Each phase is scoped so the tree keeps building and running (`build-cmake.bat` on Windows / `cmake --build build` elsewhere) before moving on.

## Unified Modernization Roadmap (SDL + Utilities)
| Stage | Origin | Focus | Status | Notes |
| --- | --- | --- | --- | --- |
| S0 | Prop Phase 0 | Baseline build + warning log | ✅ Done | Guardrails recorded in `session_notes.md:37-124`. |
| S1 | SDL Phase 1 | Highscore I/O rewrite + flush safety | ✅ Done | SDL-only `highscore_add()`/`open_scores_file_versioned()` (see `session_notes.md:374-512`). |
| S2 | SDL Phase 2 | Remove `USE_SDL` conditionals + legacy modules | ✅ Done | `main.c`, CMake, and build scripts now assume SDL exclusively (`session_notes.md:4870-4890`). |
| S3 | Prop Phase 1 | String/memory helper cleanup | ✅ Done | Inline `streq/prefix/suffix`; `z-virt` migrated to SDL allocators. |
| S4 | Prop Phase 2 | SDL-backed file/path wrappers adoption | ✅ Done | `sdl_fopen` + friends landed in `src/fs/io_sdl.c`; dump/load callers switched. |
| S5 | Prop Phase 3 | Formatting/logging glue overhaul | ✅ Done | `format.c/.h` own the API; `z-form.*` removed. |
| S6 | Prop Phase 4/4b | RNG + math helper migration | ✅ Done | `rng.c/.h` + SDL random context integration. |
| S7 | Prop Phase 4c + SDL Phase 3/4 | Filesystem breakout + modern error contracts | ⚙️ In progress | SDL path helpers return `bool`; `init1.c`, `squelch.c`, and the metarun maintenance now honor their errors. Next up: finish migrating the remaining loaders (`init2.c`, `cmd4.c` pref walkers) to the new helpers. |
| S8 | Prop Phase 5 | Terminal abstraction retirement | 📝 Planned | `z-term` refactor tracked in its own plan; keep stable until SDL panes cover all flows. |
| S9 | Prop Phase 6 + SDL Phase 5 | Final utility deletion + regression matrix | 📝 Planned | Requires spoiler/dump verification, metarun backups, and removal of unused `z-*` files. |

**Immediate next actions**
1. Leverage the new `bool`-returning filesystem helpers to tighten error handling in the remaining loaders (`init2.c` follow-ups, `cmd4.c` dumps, other pref walkers).
2. Port the remaining loaders to focused `fs/*` helpers as part of the filesystem breakout.
3. Re-run the dump/spoiler/metarun regression matrix once the filesystem helpers stabilize, then proceed to the `z-term` plan.

## Status Snapshot (2025-11-11)
| Phase | Focus | Status | Evidence |
| --- | --- | --- | --- |
| 0 | Baseline + guardrails | Done | Baseline build + warning count recorded in `session_notes.md:37-124`. |
| 1 | String and memory helpers | Done | `streq/prefix/suffix` now inline in `src/angband.h:94-110`; `z-virt.c` uses SDL allocators; logged in `session_notes.md:39-134`. |
| 2 | SDL-backed file/path utilities | Done | `sdl_fopen` and friends live in `src/fs/io_sdl.c` and are used throughout loaders/dumps (`src/cmd4.c:170-260`, `src/save.c:316-2021`). |
| 3 | Formatting + logging glue | Done | `src/format.c`/`src/format.h` own the public helpers (`session_notes.md:4235-4277`); `z-form.*` has been deleted. |
| 4 | RNG + math helpers | Done (compatibility baseline) | `src/rng.c`/`src/rng.h` replace `z-rand.*` (`session_notes.md:3-84`) while preserving deterministic behavior. |
| 4b | SDL random state integration | Done | Single SDL-backed RNG state (`Rand_state_export/import`) now drives all random draws (`session_notes.md:4203-4216`). |
| 4c | Filesystem breakout | In progress | SDL path helpers live in `src/fs/path.c`; `path_parse/path_build/path_temp/fd_*` now return `bool` (2025-11-11). Next up: migrate `init1/2`, `squelch`, and metarun maintenance to the helpers (session_notes.md, 2025-11-11). |
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
| `z-form.c/h` | **RETIRED** — functionality merged into `src/format.c`. | n/a |
| `z-rand.c/h` | Replaced by `src/rng.c/rng.h`; still uses bespoke LCRNG arrays internally. | Finish Phase 4b by backing RNG state with `SDL_RandomContext` instances while keeping deterministic seeds. |
| `z-term.c/h` | Terminal/window abstraction predating SDL panes. | Route rendering/input through SDL view helpers and delete the terminal layer. |
| `util.c` | Path parsing (`path_parse` at `src/util.c:129-378`), filesystem helpers, logging init, color utilities, and miscellaneous shims. | Split into focused modules (filesystem, logging bootstrap, color helpers, birth helpers) so the last legacy pieces can be deleted alongside `z-*`. |

## Active Migration Roadmap
Each phase should end with a clean SDL3 build and a smoke run (new game -> spend a few turns -> quit). Later phases also require targeted playtest scenarios (combat history, story font overlays, etc.).

### Phase 3 - Replace Formatting + Logging Glue (`z-form`, `plog/quit/core`)
**Status:** Done (cleanup pending)  
**Scope:** Consolidate on SDL/C17 formatting while centralizing logging/quit hooks.
- Added `src/format.c`/`src/format.h` with `format()`, append helpers, and logging-safe wrappers so callers stop depending on `z-form.h`.
- Remaining work (now part of Phase 5) is to move `strnfmt/vstrnfmt/strnfcat` into `format.c`, delete the static format buffer in `z-form.c:600-642`, and remove the last `plog_fmt/quit_fmt/core_fmt` wrappers once everything uses `log/log.h`.

### Phase 4 - RNG + Math Modernization (`rng.c`, probability helpers)
**Status:** Done (compatibility baseline)  
**Scope:** Provide a modern home for RNG APIs without altering gameplay outcomes.
- Created `src/rng.c`/`src/rng.h` and updated all includes to use the new module; `z-rand.*` has been deleted.
- Gameplay continues to use the same deterministic algorithms, so saves remain compatible while we prepare to adopt SDL’s RNG internals in Phase 4b.

### Phase 4b - SDL Random State Integration
**Status:** Done  
**Scope:** Collapse legacy RNG globals into a single SDL-backed state.
- Introduced `Rand_state_export()`/`Rand_state_import()` so callers and savefiles can snapshot/restore the 64-bit state deterministically.
- `Rand_div()`/`Rand_normal()` now draw from `SDL_rand_bits_r()`; save/load writes the 64-bit state into the legacy block layout so existing files continue to deserialize correctly.
- Deterministic helpers (`flavor_init`, `randart.c`, `monster2.c`) now save/restore the state instead of toggling `Rand_quick`/`Rand_value`, and new-game seeding always calls `Rand_state_init()` with a 64-bit seed derived from `time()`/`SDL_GetPerformanceCounter()`.

### Phase 5 - Filesystem, Logging & Color Cleanup
**Status:** Planned  
**Scope:** Break util.c into focused modules so the last `z-*` dependencies disappear.
- SDL IO helpers (`sdl_fopen`, path builders) now live in `src/fs/io_sdl.c`/`fs/io_sdl.h`; follow-up work will replace `path_parse/path_temp` with SDL-aware helpers.
- Move logger bootstrap (`init_logger`, log path discovery, atexit handlers) into `src/logging/bootstrap.c`, leaving only prototypes for the rest of the tree.
- Relocate color/text helper tables (e.g., `short_color_names`, `attr_to_text`, UI string helpers) into `src/ui/colors.c` so UI code owns palette/presentation data.
- Result: `util.c` shrinks toward birth/helpers only, clearing the runway for final `z-*` removal.

### Phase 6 - Final Cleanup & z-* Deletion
**Status:** Planned  
**Scope:** Remove the retired modules and normalize the tree (SDL terminal work tracked separately).
- Delete remaining `z-*.c/.h` once their responsibilities are absorbed; stop including them from `angband.h` and `externs.h`.
- Update docs (`README.md`, `SDL_MIGRATION*.md`, `session_notes.md`) and build files to reflect the final module layout; ensure no stale references remain.
- Finish with a clean build/test sweep and (ideally) a reduced warning count thanks to the slimmer abstraction layer.

## File Restructuring & z-* Retirement
1. **Strings and memory (cleanup pending):** Inline helpers live in `src/angband.h:94-110`, but `z-util.c` still contains dead `my_str*` definitions. Remove them once all callers include `format.h`/`angband.h` instead of `z-util.h`.
2. **Filesystem breakout:** Move the SDL IO helpers from `src/util.c:299-629` into `src/fs/io_sdl.c` + `fs/io_sdl.h` so consumers (`cmd4.c`, `save.c`, etc.) no longer need the entire `util.c` surface. SDL path helpers now live in `src/fs/path.c`, providing `SDL_GetUserFolder`-backed `path_parse/path_temp` plus `fd_*` wrappers; next step is to migrate callers off `util.c` entirely.
3. **Logging/bootstrap module:** Extract `init_logger()` and related helpers from the bottom of `util.c` into `src/logging/bootstrap.c`, leaving only prototypes in `externs.h` and making teardown flows (`atexit`, `SDL_Quit`) consistent.
4. **Color and text helpers:** Relocate `short_color_names`, `attr_to_text`, and related UI tables into `src/ui/colors.c`. This isolates UI data from `util.c` and primes us to delete `z-term` once panes own the palette logic.
5. **Filesystem/logging split:** Break SDL IO and logger bootstrap helpers out of `util.c` into dedicated modules so the remaining util code focuses on gameplay/birth helpers.
6. **Color/text helpers:** Move palette lookup tables and UI string helpers into `src/ui/colors.c` (and related files) so UI components own their data.
7. **RNG cleanup:** With the single-state SDL RNG in place, add convenience push/pop helpers for deterministic subsequences and finish documenting the 64-bit serialization format.
8. **Terminal abstraction:** Create `src/ui/term_sdl.c` to house any remaining terminal-like behaviors, then remove `z-term.c/h` once SDL panes cover the rendering/input lifecycle (tracked as a separate effort).
9. **Header hygiene:** Stop including `z-*.h` from `angband.h` once functionality migrates. Replace macros (`C_MAKE`, `KILL`, etc.) with typed inline helpers or enums to leverage C17 diagnostics.

## C17 Modernization Checklist
- **`z-util.c:24-119`** — Delete the unused `my_stricmp/my_strnicmp` implementations and strip their prototypes from `z-util.h`.
- **`z-virt.h:32-86`** — Replace macro-heavy allocation helpers with typed inline wrappers that return `bool` or `size_t` for clearer error handling.
- **`src/rng.c`** — Document the new `Rand_state_export()/import()` helpers, add push/pop APIs for deterministic subsequences, and ensure save tools understand the 64-bit state format.
- **`z-term.c`** — Continue carving UI helpers into SDL-aware modules so the Angband terminal abstraction can be deleted.
- **`src/util.c:129-742`** — Replace `path_parse/path_temp` with SDL path helpers; drop historical tilde and drive parsing logic.
- **Global logging/quit hooks** — Collapse duplicate `plog/quit/core` implementations into a single `log_fatal()` path that flushes via `log/log.h` and calls `SDL_Quit`.

## Coordination Notes
- Keep `session_notes.md` updated with phase status, blockers, and verification evidence (per AGENTS guide).
- Stage work so gameplay-critical files (`dungeon.c`, `cmd*.c`) only see mechanical changes in early phases; defer functional simplifications until after the utility surface is gone.
- Reuse the existing logging infrastructure (`log/log.h`) for diagnostics instead of ad-hoc `printf`s while refactoring.

Following this plan moves us from decades-old portability shims to a lean SDL3/C17 codebase without destabilizing core gameplay between steps.
