# Proprietary Utility Retirement Plan

This document inventories the legacy helper layers (primarily `z-*.c` and `util.c`) and lays out a stepwise refactor path that replaces them with modern C17 or SDL3 facilities. Each phase is scoped so the tree keeps building and running (`build-cmake.bat` on Windows / `cmake --build build` elsewhere) before moving on.

## Status Snapshot (2025-11-10)
| Phase | Focus | Status | Evidence |
| --- | --- | --- | --- |
| 0 | Baseline + guardrails | Done | Baseline build + warning count recorded in `session_notes.md:37-124`. |
| 1 | String and memory helpers | Done | `streq/prefix/suffix` now inline in `src/angband.h:94-110`; `z-virt.c` uses SDL allocators; changes logged in `session_notes.md:39-134`. |
| 2 | SDL-backed file/path utilities | Done | `sdl_fopen`/friends live in `src/util.c:299-629` and are consumed by loaders/dumps (e.g., `src/cmd4.c:170-260`, `src/dump_items.c:80-949`). |
| 3 | Formatting + logging glue | Planned | Replace `z-form` and vararg wrappers with SDL/C17 primitives. |
| 4 | RNG + math helpers | Planned | Move to SDL random contexts and consolidate probability helpers. |
| 5 | Terminal abstraction | Planned | Collapse `z-term` once SDL panes cover all rendering/input needs. |
| 6 | Final deletion | Planned | Remove remaining `z-*` files and normalize headers/docs. |

## Goals
- Retire bespoke portability shims (string/memory helpers, RNG, term package, path/file wrappers) that date back to pre-SDL ports.
- Lean on SDL3 (windowing, input, timing, RNG, file I/O helpers) or the C17 standard library to simplify maintenance.
- Delete dead platform code; SDL3 is now the only supported frontend (`session_notes.md:4115-4138`), so we can excise residual guards instead of keeping compatibility abstractions.
- Adopt modern C17 practices (explicit sizing, `bool`, `size_t`, `const`, error propagation) as we touch each subsystem.
- Restructure files so functionality lives in focused modules rather than monolithic legacy buckets.

## Codebase Snapshot (2025-11)
- `src/` - gameplay core (`cmd*.c`, `object*.c`, `dungeon.c`, etc.), SDL-only frontend (`main-sdl.c`), and legacy utility code (`z-*.c`, `util.c`, `files.c`). The old `main-gcu.c` and `main-win.c` code paths are gone.
- `lib/` - text data (`lib/edit/*.txt`), prefs (`lib/pref/*.prf`), and fonts/palettes (`lib/pref/font-*.prf`).
- `sil-more-windows-sdl3/` - deployment payload (game assets plus `log.txt` runtime output).
- Docs/scripts - migration notes (`SDL_CLEANUP_PLAN.md`, `SDL_MIGRATION.md`), the evolving `session_notes.md`, and helper scripts under `tools/` or repo root.

Despite the SDL-only pivot, the gameplay loop still routes through the Angband-era `term` abstraction, bespoke random/formatting helpers, and a monolithic `util.c`. Those are the remaining seams we are collapsing.

## Legacy Utility Inventory

| Module | Responsibilities today | Replacement direction |
| --- | --- | --- |
| `z-util.c/h` | Residual case-insensitive compare helpers (`z-util.c:24-119`) that no longer have call sites plus historical logging hooks. | Delete the unused functions and move any still-needed declarations into modern headers; rely on SDL/standard logging plus `log/log.h`. |
| `z-virt.c/h` | Allocation macros (`C_MAKE`, `KILL`, etc.) and wrappers around SDL memory calls. | Replace macros with explicit helpers or typed inline functions; migrate remaining call sites to standard/SDL allocators so the file can be deleted. |
| `z-form.c/h` | Custom `strnfmt`, `vformat`, and `plog/quit/core` varargs wrappers built on a static buffer (`z-form.c:600-642`). | Switch to `SDL_snprintf`/`SDL_vsnprintf`, keep only thin helpers where necessary, and reroute error handling through `log/log.h`. |
| `z-rand.c/h` | Dual RNG engines, probability helpers (`Rand_int`, `Rand_normal`, `div_round`). | Back randomness with `SDL_CreateRandomContext` and deterministic seeds; preserve helper APIs while transitioning internals. |
| `z-term.c/h` | Terminal/window abstraction that predates SDL panes. | Derive a SDL-native UI API, migrate callers off `Term_*`, and then delete `z-term.*` in favor of SDL view structs. |
| `util.c` | Path parsing (`path_parse` at `src/util.c:129-378`), filesystem helpers, logging init, color utilities, and various leftover platform shims. | Carve this into focused modules (filesystem, logging bootstrap, color tables, birth helpers) so we can drop unused decades-old code in parallel with `z-*` removals. |

## Active Migration Roadmap
Each phase should end with a clean SDL3 build and a smoke run (new game -> spend a few turns -> quit). Later phases also require targeted playtest scenarios (combat history, story font overlays, etc.).

### Phase 3 - Replace Formatting + Logging Glue (`z-form`, `plog/quit/core`)
**Status:** Planned
**Scope:** Consolidate on SDL/C17 formatting and centralize quitting/logging hooks.
- Replace `vstrnfmt/strnfmt/strnfcat` with wrappers over `SDL_vsnprintf` while keeping the same truncation semantics needed by lore dumps and message formatting.
- Remove the static global format buffer (`z-form.c:600-642`) in favor of stack buffers or caller-supplied scratch arenas; this removes thread-safety issues and simplifies lifetime management.
- Redirect `plog/quit/core` to `log/log.h` plus SDL quit hooks, ensuring fatal SDL paths still flush logs before exit.
- Delete any `extern` declarations for those helpers from `z-form.h`/`z-util.h` once migrated.

### Phase 4 - RNG + Math Modernization (`z-rand`, probability helpers)
**Status:** Planned
**Scope:** Swap RNG internals while preserving gameplay distributions.
- Introduce a new `rng.c` that seeds one or more `SDL_RandomContext` instances (global RNG, quick RNG for spell effects, UI-safe RNG) and exposes deterministic helpers (`rng_uniform`, `rng_percent`, `rng_normal`, `rng_choice`).
- Migrate `Rand_quick`, `Rand_simple`, and math helpers like `div_round` from `z-rand.c` into the new module, keeping API compatibility while tests cover distribution drift.
- Add statistical smoke tests (standalone executable or scripted run) so we can compare histograms before/after the swap.

### Phase 5 - Collapse `z-term` in Favor of SDL Views
**Status:** Planned
**Scope:** Remove the Angband terminal abstraction once SDL panes cover every feature.
- Catalogue every remaining `Term_*` usage and map it to an SDL pane responsibility (message log, combat rolls overlay, unified look UI, etc.).
- Add SDL-native helpers (render queues, cursor draw, input dispatch) that let gameplay code target SDL directly without bouncing through `term` structs.
- Once gameplay code calls SDL helpers, delete the unused `term` flags, screen images, and queueing logic in `z-term.c` and shrink `term` to an internal SDL view struct.
- Regression-test UI-heavy flows: combat history, inventory/equipment overlays, unified look, Steam Deck shortcuts, auto list drop-downs.

### Phase 6 - Final Cleanup & z-* Deletion
**Status:** Planned
**Scope:** Remove the retired modules and normalize the tree.
- Delete `z-*.c/.h` once their responsibilities are absorbed; stop including them from `angband.h` and `externs.h`.
- Break up `util.c` into focused modules (filesystem, logger bootstrap, color tables, birth helpers) so any stragglers from earlier phases move into obvious homes.
- Update docs (`README.md`, `SDL_MIGRATION*.md`, `session_notes.md`) and build files to reflect the new home for helpers; ensure no stale references to removed symbols remain.
- Finish with a clean build/test sweep and (ideally) compiler warnings reduced thanks to the slimmer abstraction layer.

## File Restructuring & z-* Retirement
1. **Strings and memory (complete, cleanup pending):**
   - Inline helpers live in `src/angband.h:94-110`, but `z-util.c` still contains dead `my_str*` definitions. Delete the file after confirming no link-time references remain and move any logging hooks directly into `log/log.h` or SDL glue.
2. **Filesystem breakout:**
   - Move `sdl_fopen` and friends from `src/util.c:299-629` into `src/fs/io_sdl.c` (new TU) with a matching header so consumers (`cmd4.c`, `dump_items.c`, `init1.c`, `save.c`, etc.) stop including the entire `util.c` surface.
   - Keep `path_parse`/`path_temp` in the same TU temporarily, then replace with SDL path helpers so we can excise the 1990s-era pathname rules.
3. **Logging/bootstrap module:**
   - Extract `init_logger()` and related helpers from the bottom of `util.c` into `src/logging/bootstrap.c`, leaving only high-level entry points in `main-sdl.c`.
4. **Color and text helpers:**
   - Relocate color tables (`short_color_names`, `attr_to_text`, etc.) into `src/ui/colors.c`. This makes it easier to delete `z-term` once panes own all palette logic.
5. **z-form/z-rand/z-term retirement:**
   - Create new modules (`format.c`, `rng.c`, `ui/term_sdl.c`) and migrate callers incrementally. After each migration, remove the corresponding chunk from the old `z-*` file so we can delete the file instead of leaving dead code behind.
6. **Header hygiene:**
   - Stop including `z-*.h` from `angband.h` once functionality moves. Replace macros with `static inline` functions or typed enums to better leverage compiler diagnostics.

## C17 Modernization Checklist
- **`z-util.c:24-119`** - Remaining `my_stricmp/my_strnicmp` functions are unused. Delete them and drop the header declarations to avoid stale prototypes.
- **`z-form.c:600-642`** - Static global format buffer uses manual `C_MAKE/KILL` macros and is not thread-safe. Replace with stack buffers or caller-provided storage backed by `SDL_vsnprintf`.
- **`z-virt.h:32-86`** - Macro-heavy allocation helpers hide types and make static analysis difficult. Replace with typed inline wrappers or dedicated helper functions that return `bool`/`size_t` for clearer error handling.
- **`z-term.c` (entire TU)** - Uses Angband-era types (`byte`, `s16b`), manual screen images, and hook tables that assume multiple backends. Once SDL is the only frontend, collapse these into modern structs with explicit ownership, `bool` flags, and `size_t` counts.
- **`src/util.c:129-742`** - `path_parse/path_temp` still implements home-directory (`~user`) rules for platforms we no longer target. Replace with SDL's path helpers or thin wrappers over standard filesystem APIs.
- **Global logging/quit hooks** - `plog/quit/core` duplication should be replaced with a single `log_fatal()` path that always flushes via `log/log.h` and then calls `SDL_Quit`. This ensures consistent teardown behavior and removes custom crash pokes.

## Coordination Notes
- Keep `session_notes.md` updated with phase status, blockers, and verification evidence (per AGENTS guide).
- Stage work so gameplay-critical files (`dungeon.c`, `cmd*.c`) only see mechanical changes in early phases; defer functional simplifications until after the utility surface is gone.
- Reuse the existing logging infrastructure (`log/log.h`) for diagnostics instead of ad-hoc `printf`s while refactoring.

Following this plan moves us from decades-old portability shims to a lean SDL3/C17 codebase without destabilizing core gameplay between steps.
