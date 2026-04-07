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
| **S7** | Filesystem breakout + utility split | In progress | Story-font helpers still live in `src/util.c`, and we have not captured regression evidence for the bool-returning filesystem contract. |
| **S9** | Final utility deletion + C17 polish | In progress | `z-term` is still globally exported and we have yet to delete the legacy terminal layer or rerun the post-cleanup regression matrix. |

_Stage S8 (terminal abstraction + SDL panes) now lives in `SDL_CLEANUP_PLAN.md` so we can finish S7 and S9 before touching the rendering stack again._

### Stage S7 - Filesystem breakout & utility split (In progress)
1. **Honor the bool-returning filesystem contract everywhere.** (Done) `cmd4.c:8568-8688`, `wizard1.c:143-230`, `metarun.c:835-1282`, and `squelch.c:236-340` now log failures and bail whenever `path_build()`/`path_temp()` or `sdl_fopen()` fails, preventing partially written files.
2. **Consolidate filesystem helpers under `src/fs/`.** (Done) Dumping/spoiler/metarun files include `fs/io_sdl.h` and `fs/path.h` directly (`src/cmd4.c:12-20`, `src/wizard1.c:12-18`, `src/metarun.c:10-21`), so no TU relies on transitive `util.h` exports.
3. **Split `util.c` by responsibility.** (In progress) The color tables (`src/ui/colors.c:7-31`) and `init_logger()` (`src/log/bootstrap.c:15-110`) were split out, but the story-font overlay (`src/util.c:2553-2974`) still lives in util-land and needs a dedicated `ui/story_font.c`.
4. **Remove duplicate string helpers from `z-util`.** (Done) The inline helpers in `src/angband.h:85-122` are now the sole implementations and `z-util.*` has been deleted.
5. **Unify error/quit paths.** (Done) `plog/quit/core` live inside `src/log/fatal.c:1-69`, route through `log/log.h`, and register SDL quit hooks so shutdown happens exactly once.
6. **Retire the score-file singleton.** (Done) `score_file_ctx` plus `score_file_active_ctx()` (see `src/score/score_io.c:13-40` and the macros at `src/files.c:4610-4615`) replaced the `highscore_fd` globals.
7. **Document + test path-dependent flows.** (In progress) Need to record the dump/spoiler/metarun regression matrix in `session_notes.md` now that the bool contract is enforced.

### Stage S9 - Final utility deletion + C17 polish (In progress)
1. **Modernize `z-virt`.** (Done) `mem/alloc.h:1-45` provides the typed helpers and the legacy `z-virt.*` files have been deleted.
2. **Extend the RNG API.** (Done) `src/rng.c:12-48` documents the serialization format and ships `Rand_state_push()`/`Rand_state_pop()` for deterministic previews.
3. **Collapse global logging/quit hooks.** (Done) `src/log/fatal.c:1-69` owns `plog/quit/core`, calls into `log/log.h`, and ensures SDL shutdown happens exactly once.
4. **Header hygiene.** (In progress) `angband.h:24-33` still re-exports `z-term.h`; we need SDL pane headers and per-module includes before we can drop the terminal singleton.
5. **Final deletion + regression matrix.** (In progress) After the story-font helpers move out of `util.c` and SDL panes stop using the global `Term`, delete the remaining `z-*` code paths and rerun the spoiler/dump/metarun plus SDL panes QA suites.

## Bad Practice Remediation Targets
- **Global logging/exit state (`src/log/fatal.c`)** - Done. `plog/quit/core` now live in `log/fatal.c`, route through `log/log.h`, and support multiple registered quit hooks; the legacy globals from `z-util.c` are gone.
- **Implicit score-file singletons (`src/files.c:4610-5861`, `src/score/score_io.c:13-40`)** - Done. `score_file_ctx` owns every score handle so tests and metarun tooling can swap contexts without hidden globals.
- **Header-level global exposure (`src/angband.h:24-33`)** - In progress. `z-term.h` is still re-exported globally; SDL pane headers and narrower `externs.h` slices are still required.
- **UI palette/text globals (`src/ui/colors.c:7-31`)** - Done. Palette/text tables now live under `src/ui/colors.c` with `static` storage.
- **Terminal singleton (`src/z-term.c:273`)** - In progress. The global `term* Term` still leaks through the UI; SDL pane context work (tracked in `SDL_CLEANUP_PLAN.md`) remains to be done.

## File Restructuring & z-* Retirement Tracker
1. **Strings/memory helpers (Done):** `streq/prefix/suffix` live inline in `angband.h`, and the remaining SDL string helpers were moved into `src/support/strl.c` so `z-util.*` could be deleted entirely.
2. **Filesystem breakout (Done):** SDL IO helpers live in `src/fs/io_sdl.c` and are widely used (`cmd4.c`, `init2.c`, `save.c`, `metarun.c`).
3. **Logging/bootstrap module (Done):** `init_logger()` moved into `src/log/bootstrap.c`.
4. **Color/text helpers (Done):** Palette/text tables sit under `src/ui/colors.c` with a dedicated header.
5. **Filesystem/logging split (Done):** All modules include `fs/*` and `log/*` directly; `angband.h` no longer re-exports them.
6. **Palette/text duplication (Done):** See item #4.
7. **RNG cleanup (Done):** Push/pop helpers plus documentation now live in `src/rng.c`.
8. **Terminal abstraction (Planned):** `z-term.c` remains the active backend; create `ui/term_sdl.c` and retire the legacy abstraction per Stage S8.
9. **Header hygiene (In progress):** `angband.h:24-33` still re-exports `z-term.h`; finish auditing the SDL pane headers so we can drop that include and keep chipping away at `z-term`.

## C17 Modernization Checklist
- [x] **`z-util.c:24-119` - Remove unused `my_str*` helpers.** (Confirmed absent as of 2025-11-12.)
- [x] **`z-virt.h:32-110` - Replace macro-heavy allocators with typed inline wrappers** that return explicit success/failure codes. (All `C_*` macros removed; callers now rely on `mem_alloc_*` + standard `memset/memcpy`.)
- [x] **`src/rng.c` - Document `Rand_state_export/import`, add push/pop helpers, and validate the 64-bit serialization format.**
- [ ] **`z-term.c` - Continue carving UI helpers into SDL-aware modules until the abstraction can be deleted.**
- [x] **`src/util.c:129-742` - Move `path_parse/path_temp` into SDL path helpers** (now lives in `src/fs/path.c`).
- [x] **Global logging/quit hooks - Collapse `plog/quit/core` into a single `log_fatal()` path that flushes via `log/log.h` and calls `SDL_Quit`.**

## Immediate Next Actions
1. Wire the new `score_file_ctx` through the highscore helpers so tests (and metarun tools) can open isolated contexts without touching the default instance.
2. Shrink `externs.h` into targeted headers (monster/object/player/etc.) now that no TU includes it implicitly.
3. Begin carving `z-term.c` into SDL-native panes per Stage S8 once the above cleanup lands.

## Coordination Notes
- Keep `session_notes.md` updated with phase status, blockers, and verification evidence (per AGENTS guide).
- Stage work so gameplay-critical files (`dungeon.c`, `cmd*.c`) only see mechanical changes in early phases; defer functional simplifications until after the utility surface is gone.
- Reuse the existing logging infrastructure (`log/log.h`) for diagnostics instead of ad-hoc `printf`s while refactoring.
- Re-test inventory/equipment/unified-look flows after each UI/term change; highlight alignment bugs immediately.

