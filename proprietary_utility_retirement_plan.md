# Proprietary Utility Retirement Plan

This document inventories the legacy helper layers (primarily `z-*.c` and `util.c`) and lays out a stepwise refactor path that replaces them with modern C17 or SDL3 facilities. Each phase is scoped so the tree keeps building and running (`build-cmake.bat` on Windows / `cmake --build build` elsewhere) before moving on.

## Goals
- Retire bespoke portability shims (string/memory helpers, RNG, term package, path/file wrappers) that date back to pre-SDL ports.
- Lean on SDL3 (windowing, input, timing, RNG, file I/O helpers) or the C17 standard library to simplify maintenance.
- Delete dead platform code (`SET_UID`, `main-gcu.c`, `main-win.c`, etc.) while keeping the SDL3 front end first-class.
- Adopt modern C practices (explicit sizing, `bool`, `size_t`, `const`, error propagation) as we touch each subsystem.

## Codebase Snapshot (2025-11)
- `src/` — gameplay core (`cmd*.c`, `object*.c`, `dungeon.c`, etc.), platform glue (`main-sdl.c`, `main-win.c`, `main-gcu.c`), and the legacy utility layer (`z-*.c`, `util.c`, `files.c`).
- `lib/` — text data (`lib/edit/*.txt`, UI pref files, fonts).
- `sil-more-windows-sdl3/` — SDL deployment payload (logs land in `log.txt` here).
- Scripts/docs — migration notes (e.g., `SDL_CLEANUP_PLAN.md`, `SDL_MIGRATION.md`, `session_notes.md`).

The codebase still mirrors historical Angband layering: everything (even SDL) talks through `term` and bespoke helpers. That is the surface we are collapsing.

## Legacy Utility Inventory

| Module | Responsibilities today | Replacement direction |
| --- | --- | --- |
| `z-util.c/h` | `my_str*`, `streq/prefix/suffix`, logging/quit hooks | Use `<string.h>`, `<strings.h>` (or SDL string helpers) and `SDL_Log`/`log_*` directly; wire graceful shutdown straight to SDL/platform exit paths. |
| `z-virt.c/h` | `ralloc`, macros (`C_MAKE`, `WIPE`, etc.), `string_make/free` | Replace macros with typed helpers or direct `calloc/free`; prefer `SDL_malloc`, `SDL_calloc`, `SDL_free` for consistency with SDL subsystems. |
| `z-form.c/h` | Custom `strnfmt`, `vformat`, and `plog/quit/core` varargs wrappers | Use `SDL_snprintf`/`SDL_vsnprintf` (or standard `snprintf`) plus local helpers for recurring patterns; drop bespoke formatting codes we no longer use. |
| `z-rand.c/h` | Dual RNGs, probability helpers (`rand_int`, `Rand_normal`, etc.) | Migrate to `SDL_GetRandomNumber64`/`SDL_GetRandomBytes` with deterministic contexts (`SDL_CreateRandomContext`); keep probability helpers but back them with SDL contexts. |
| `z-term.c/h` | Platform-neutral terminal abstraction | Route rendering/input directly through SDL view/pane code in `main-sdl.c`; collapse `term` when SDL-only path is feature-complete. |
| `util.c` | Filesystem shims (`my_fopen`, `path_*`, `fd_*`), OS gating (`SET_UID`), logging init, text helpers, color tables | Replace file APIs with `SDL_IOStream` or plain stdio, remove unused UNIX credential code, and split remaining responsibilities into focused C17 modules (filesystem, serialization, color, logging). |

## Phased Migration Strategy
Each phase should end with a clean SDL3 build and a smoke run (new game → spend a few turns → quit) to keep regressions contained. The order goes from least disruptive to most invasive.

### Phase 0 – Baseline + Guardrails
**Scope:** CI/build hygiene before touching code.
- Ensure `build-cmake.bat` (Windows) and `cmake --build build` (Linux/macOS) succeed from a clean tree; capture compiler warnings for guidance.
- Enable compiler warnings we currently suppress (e.g., `/W4` or `-Wall -Wextra`) and log the noise level; this informs later refactors.
- Document current runtime dependencies (fonts, INI files, save/log locations) so later deletions do not strand assets.

**Verification:** Successful rebuild + manual SDL launch; zero unexpected warnings documented in `session_notes.md`.

### Phase 1 – Retire Simple String/Memory Wrappers (`z-util`, `z-virt`)
**Scope:** Drop helpers that are 1:1 with standard/SDL calls.
- Replace `my_stricmp/my_strnicmp` with `SDL_strcasecmp/SDL_strncasecmp` (or `strncasecmp` behind a portability shim).
- Swap `my_strcpy/my_strcat/streq/prefix/suffix` for direct `<string.h>`/`<strings.h>` helpers; add lightweight inline wrappers in `angband.h` if needed for readability.
- Replace `C_MAKE`/`MAKE` and friends with explicit `SDL_calloc`/`SDL_free` (or `memset`); introduce small local helpers for common idioms (e.g., `xalloc_array(type, count)`).
- Delete `z-util.c`/`z-virt.c` references from any TU as it migrates; keep the files until all call sites move.

**Verification:** Rebuild + run basic gameplay; pay attention to any allocation hot paths (object generation, save/load).

### Phase 2 – Modernize File & Path Utilities (`util.c`, `files.c`, `cmd4.c`, `dump_items.c`)
**Scope:** Remove `my_fopen`, `my_fopen_temp`, and old fd-based APIs.
- Introduce a single SDL-backed file helper (e.g., `SDL_IOStream* fs_open_read(const char* path)` plus wrappers that return `FILE*` only when SDL lacks features we need).
- Update consumers (`cmd4.c`, `dump_items.c`, spoiler writers, logging) to use the new helper.
- Delete legacy UNIX-only code paths (`SET_UID`, password lookups, `path_parse` tilde expansion) once no caller depends on them.
- Split `util.c` into thematic units (filesystem, logging, color, sound) to keep future diffs tight.

**Verification:** Regression-test save/load, character dumps, and spoiler generation; confirm `sil-more-windows-sdl3/log.txt` is still produced via the new helper.

### Phase 3 – Replace Formatting + Logging Glue (`z-form`, `plog/quit/core`)
**Scope:** Consolidate text formatting on standard/SDL routines.
- Audit `strnfmt`/`vstrnfmt` usage; replace with `SDL_snprintf` / `SDL_vsnprintf` (or `snprintf`) plus helper wrappers where we need truncation lengths returned.
- Route `plog*`, `quit*`, `core*` through the existing `log/log.h` system and SDL's quit hooks; remove bespoke format-buffer caching.
- Ensure fatal paths inside SDL (e.g., window init) call `sdl_quit_hook` and flush logs before exiting.

**Verification:** Build and trigger representative logging/quit paths (e.g., invalid INI file, manual `Ctrl+Q`, deliberate crash via wizard command) to make sure text surfaces correctly.

### Phase 4 – RNG + Math Modernization (`z-rand`, probability helpers)
**Scope:** Swap the RNG engine while preserving gameplay distributions.
- Create an `rng.c` that encapsulates an `SDL_RandomContext` seeded from saves/metaruns; expose deterministic helpers (`rng_uniform(int max)`, `rng_percent(int n)`, `rng_normal(mean, stddev)`).
- Port all `Rand_*`, `rand_int`, `one_in_`, `div_round`, etc., to call the new engine; keep helper APIs identical initially to avoid touching gameplay code twice.
- Add regression tests (lightweight C unit test or script) that sample distributions and compare to snapshots so we catch behavior drift.

**Verification:** Simulate several auto-plays or run statistical scripts to make sure drop rates, combat rolls, and monster AI randomness stay within tolerance.

### Phase 5 – Collapse `z-term` in Favor of SDL Views
**Scope:** Remove the Angband terminal abstraction once SDL UI paths cover every feature.
- Inventory `term_*` entry points still used outside `main-sdl.c` (search for `Term_` in `src/`); introduce SDL-native APIs (pane draw queues, cursor, input events) that those callers can use instead.
- Gradually redirect callers (e.g., message display, inventory overlays, look command) to the SDL pane layer, deleting the matching `Term_*` usage.
- Once no gameplay module references `z-term.h`, remove the file and shrink `Term` struct usage inside SDL backend to plain view/pane state.

**Verification:** Full manual UI sweep (combat log, inventory/equipment overlays, look/unified targeting, combat history, Steam Deck shortcuts); keep an eye on redraw contracts (`p_ptr->redraw`, `handle_stuff()`).

### Phase 6 – Final Cleanup & Deletion
**Scope:** Remove the retired modules and old platform targets.
- Delete `z-*.c/.h`, `main-gcu.c`, `main-win.c`, and any dead data files that only those targets consumed.
- Normalize headers: stop including `z-*` files from `angband.h`, trim macros from `h-basic.h`, and ensure every TU includes the minimal standard headers it needs.
- Update docs (`README.md`, `SDL_MIGRATION*.md`, `session_notes.md`) to reflect the SDL-only, modern-C baseline.

**Verification:** Final rebuild + run; lint/format the tree; ensure new developer onboarding docs only mention SDL3 + modern tooling.

## Coordination Notes
- Keep `session_notes.md` updated with phase status, blockers, and verification evidence (per AGENTS guide).
- Stage work so gameplay-critical files (`dungeon.c`, `cmd*.c`) only see mechanical changes in early phases; defer functional simplifications until after the utility surface is gone.
- Reuse the existing logging infrastructure (`log/log.h`) for diagnostics instead of ad-hoc `printf`s while refactoring.

Following this plan moves us from decades-old portability shims to a lean SDL3/C17 codebase without destabilizing core gameplay between steps.
