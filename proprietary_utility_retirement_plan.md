# Proprietary Utility Retirement Plan (SDL3 + C17 Track)

This document now focuses solely on the remaining modernization work (Stages S7-S9) while recording the evidence that Stages S0-S6 are complete. The plan integrates the original File Restructuring & z-* Retirement list plus the C17 Modernization checklist so a single roadmap can guide the rewrite toward SDL-native, C17-compliant code.

## Verified Stages (S0-S6)
- **S0 - Baseline guardrails (Done):** Build + warning logs captured in `session_notes.md:37-124`; every follow-on change has honored that baseline.
- **S1 - Highscore I/O rewrite (Done):** `files.c:6270-6415` drives `highscore_add()` entirely through `SDL_IOStream` (`SDL_WriteIO`, `SDL_FlushIO`) with explicit header rewrites, satisfying the flush-safety requirement.
- **S2 - SDL-only builds (Done):** No `USE_SDL` guards remain (`rg USE_SDL` returns empty); `angband.h` only wires the SDL frontend, and the legacy platform stubs have been removed from `src/`.
- **S3 - String/memory helper cleanup (Done):** Inline helpers live in `src/angband.h:90-118`, and `z-virt.c:33-107` now allocates via `SDL_calloc/SDL_free`. The duplicated `streq/prefix/suffix` definitions in `z-util.c:83-115` are tracked as part of the remaining utility split.
- **S4 - SDL-backed filesystem wrappers (Done):** `src/fs/io_sdl.c` and `src/fs/path.c` own `sdl_fopen/sdl_fmake` plus the bool-returning `path_parse/build/temp` helpers; callers such as `cmd4.c`, `save.c`, `metarun.c`, and `files.c` now include `fs/io_sdl.h`.
- **S5 - Formatting/logging glue (Done):** `src/format.c` / `src/format.h` house the formatting API, and `z-form.*` no longer exists in the tree (`rg --files -g 'z-form.*'` returns nothing).
- **S6 - RNG + math helper migration (Done):** `src/rng.c:1-120` centralizes the SDL-backed RNG state plus deterministic exports, and `rng.h` provides the wrappers used across gameplay. Additional push/pop helpers remain open work under Stage S9.

## Active Roadmap (S7-S9)

| Stage | Focus | Status | Key Risks |
| --- | --- | --- | --- |
| **S7** | Filesystem breakout + utility split | In progress | `path_*` callers still ignore bool failures; `util.c` retains logging/color tables; `z-util` duplicates string/logging helpers. |
| **S9** | Final utility deletion + C17 polish | Planned | `z-virt` macro layer, RNG push/pop APIs, and global logging/quit hooks must be modernized before the remaining `z-*` files can be removed. |

_Stage S8 (terminal abstraction + SDL panes) now lives in `SDL_CLEANUP_PLAN.md` so we can finish S7 and S9 before touching the rendering stack again._

### Stage S7 - Filesystem breakout & utility split (In progress)
1. **Honor the bool-returning filesystem contract everywhere.** Many loaders/dumps still ignore the return value. Examples: `cmd4.c:8554, 9253, 9421` (pref/dump writers), `wizard1.c:153-843` (spoiler generators), `metarun.c:839-1266`, and `squelch.c:236-352`. These sites must short-circuit on failure, surface the error via `msg_print`/`log_error`, and avoid partially written files.
2. **Consolidate filesystem helpers under `src/fs/`.** Callers still include `util.h` just to reach legacy prototypes. Introduce `fs/io_sdl.h` / `fs/path.h` headers where missing and drop the redundant `#include "z-util.h"` in those files once the helpers are wired directly (e.g., `wizard1.c:150-860`, `cmd4.c` top-level includes).
3. **Split `util.c` by responsibility.** The file still holds unrelated subsystems: `short_color_names`/`attr_to_text` (lines `5170-5205`), `init_logger()` + global log routing (lines `5205-5298`), story-font helpers, and misc gameplay glue. Move these into purpose-built modules (`src/ui/colors.c`, `src/logging/bootstrap.c`, `src/ui/story_font.c`) so `util.c` only contains gameplay helpers.
4. **Remove duplicate string helpers from `z-util`.** With inline versions already available in `angband.h:90-118`, delete the legacy `streq/prefix/suffix` definitions at `z-util.c:83-115` and shrink `z-util.h` accordingly. This also eliminates conflicting symbol visibility when we tighten `-Wmissing-prototypes`.
5. **Unify error/quit paths.** `plog/quit/core` still live in `z-util.c:126-205`, bypassing the structured logging pipeline. Introduce a single `log_fatal()` flow (wrapping `log/log.h`) that ensures `SDL_Quit` is called exactly once, then drop the bespoke hooks.
6. **Retire the score-file singleton.** The `highscore_fd` and `scores_file_*` globals in `files.c:4623-4705` turn score handling into hidden state. Build a scoped `score_file_ctx` that callers pass around so tests can inject in-memory streams and Stage S9 can delete the globals entirely.
7. **Document + test path-dependent flows.** Once callers stop ignoring failures, rerun the dump/spoiler/metarun regression matrix so Stage S7 closes with evidence that the bool contract is respected end-to-end.

### Stage S9 - Final utility deletion + C17 polish (Planned)
1. **Modernize `z-virt`.** Replace macros such as `C_MAKE/KILL` in `z-virt.h:32-110` with typed static inline helpers that return `bool`/`size_t`, emit diagnostics, and eliminate hidden side effects. Update all callers to honor the new signatures.
2. **Extend the RNG API.** Implement documented push/pop helpers around the SDL RNG state plus serialization comments inside `rng.c:19-40`, and teach the save/load tooling how to validate the 64-bit blobs before accepting them.
3. **Collapse global logging/quit hooks.** After Stage S7 creates `log_fatal()`, remove the legacy `argv0`, `plog_aux`, and `quit_aux` globals from `z-util.c`, and ensure shutdown flows go through `log/log.h` + SDL cleanup.
4. **Header hygiene.** Stop including `z-util.h`, `z-virt.h`, and `z-term.h` from `angband.h:18-37`. Instead, expose only the modern modules through `externs.h` or dedicated headers so files can include what they use. This is the last blocker before deleting `z-*`.
5. **Final deletion + regression matrix.** Remove the unused `z-*` files, shrink `util.c` out of existence, and rerun the spoiler/dump/metarun matrix plus the SDL panes QA list to ensure no regressions.

## Bad Practice Remediation Targets
- **Global logging/exit state (`src/z-util.c:18-205`)** – `argv0`, `plog_aux`, `quit_aux`, and `core_aux` store mutable global pointers and write directly to `stderr`. Stage S7 should fold these paths into a single `log_fatal()` helper in `log/log.h`, remove the globals, and pass process context explicitly when initializing the logger.
- **Implicit score-file singletons (`src/files.c:4623-4793`, `src/files.c:6270-6478`)** – `highscore_fd`, `scores_file_version_*`, and `scores_file_entry_count` are module-level globals that every caller mutates. Stage S7 should wrap them in a `score_file_ctx` struct that is handed to `highscore_*` helpers, enabling deterministic tests and removing hidden state.
- **Header-level global exposure (`src/angband.h:18-43`)** – Including `externs.h` from `angband.h` injects every global symbol into all translation units, making dependency tracking and linting impossible. Stage S9 should stop re-exporting `externs.h`, require modules to include the headers they actually use, and rely on forward declarations or context structs where needed.
- **UI palette/text globals (`src/util.c:5170-5205`)** – `short_color_names` and `attr_to_text` live in util-land even though only UI panes read them. Stage S7 needs to move these tables into `src/ui/colors.c` (with a dedicated header) and keep the data `static` so other subsystems cannot mutate it.
- **Terminal singleton (`src/z-term.c:273`)** – the global `term* Term` leaks the active window pointer across the entire game. The follow-on terminal plan (tracked in `SDL_CLEANUP_PLAN.md`) will replace this singleton with SDL pane context passed through UI layers once Stages S7 and S9 land.

## File Restructuring & z-* Retirement Tracker
1. **Strings/memory helpers (TODO):** Remove the redundant `streq/prefix/suffix` implementations from `z-util.c:83-115` now that `angband.h` provides inline versions.
2. **Filesystem breakout (Done):** SDL IO helpers live in `src/fs/io_sdl.c` and are widely used (`cmd4.c`, `init2.c`, `save.c`, `metarun.c`).
3. **Logging/bootstrap module (TODO):** `init_logger()` remains in `util.c:5205-5298`; move it into `src/logging/bootstrap.c` with declarations in `log/log.h`.
4. **Color/text helpers (TODO):** `short_color_names` and `attr_to_text` still live in `util.c:5170-5195`; relocate them into `src/ui/colors.c`.
5. **Filesystem/logging split (In progress):** File helpers already reside in `fs/`, but `util.c` is still included for logging/bootstrap; resolve item #3 to finish the split.
6. **Palette/text duplication (TODO):** Same as item #4; once moved, drop the stale prototypes from `externs.h`.
7. **RNG cleanup (TODO):** Add RNG push/pop helpers and state documentation inside `src/rng.c`.
8. **Terminal abstraction (Planned):** `z-term.c` remains the active backend; create `ui/term_sdl.c` and retire the legacy abstraction per Stage S8.
9. **Header hygiene (TODO):** Remove the blanket `#include "z-*.h"` usage from `src/angband.h:18-40` once dependent modules include their own headers.

## C17 Modernization Checklist
- [x] **`z-util.c:24-119` - Remove unused `my_str*` helpers.** (Confirmed absent as of 2025-11-12.)
- [ ] **`z-virt.h:32-110` - Replace macro-heavy allocators with typed inline wrappers** that return explicit success/failure codes.
- [ ] **`src/rng.c` - Document `Rand_state_export/import`, add push/pop helpers, and validate the 64-bit serialization format.**
- [ ] **`z-term.c` - Continue carving UI helpers into SDL-aware modules until the abstraction can be deleted.**
- [x] **`src/util.c:129-742` - Move `path_parse/path_temp` into SDL path helpers** (now lives in `src/fs/path.c`).
- [ ] **Global logging/quit hooks - Collapse `plog/quit/core` into a single `log_fatal()` path that flushes via `log/log.h` and calls `SDL_Quit`.**

## Immediate Next Actions
1. Update `cmd4.c`, `wizard1.c`, `metarun.c`, and `squelch.c` to bail out (and log) when `path_build` or `path_temp` fail, then rerun the dump/spoiler/metarun regression scripts.
2. Extract `init_logger()` plus the color/palette helpers into dedicated `logging/` and `ui/` modules so `util.c` stops exporting unrelated globals.
3. Replace `highscore_fd`/`scores_file_*` with a scoped context object so score operations become re-entrant and testable.
4. Remove the duplicate `streq/prefix/suffix` definitions from `z-util.c`, ensuring all translation units include `angband.h` (or a dedicated inline header) for those helpers.

## Coordination Notes
- Keep `session_notes.md` updated with phase status, blockers, and verification evidence (per AGENTS guide).
- Stage work so gameplay-critical files (`dungeon.c`, `cmd*.c`) only see mechanical changes in early phases; defer functional simplifications until after the utility surface is gone.
- Reuse the existing logging infrastructure (`log/log.h`) for diagnostics instead of ad-hoc `printf`s while refactoring.
- Re-test inventory/equipment/unified-look flows after each UI/term change; highlight alignment bugs immediately.

