# SDL-Only Code Cleanup Plan

> See the "Unified Modernization Roadmap (SDL + Utilities)" table inside `proprietary_utility_retirement_plan.md` for the cross-project ordering (Stages S0–S9). This document drills into the SDL-specific slices of those stages.

## Status Snapshot (2025-11-11)
| Phase | Focus | Status | Notes |
| --- | --- | --- | --- |
| 1 | Highscore I/O rewrite | ✅ Done | `highscore_add()` and `open_scores_file_versioned()` use SDL IO streams + flush behavior (see `session_notes.md:374-512`). |
| 2 | Remove `USE_SDL` shims | ✅ Done | All conditional compilation stripped; `src/main.c` now assumes SDL everywhere and build scripts no longer pass `-DUSE_SDL/-DUSE_GCU`. |
| 3 | C17 modernization | ⚙️ In progress | Formatting/log glue complete (`format.c`), but filesystem helpers still return `errr` and use legacy error codes. |
| 4 | Filesystem breakout | ⚙️ In progress | SDL-backed `fs/io_sdl.c` + `fs/path.c` land; remaining callers in `init1.c`, `init2.c`, `squelch.c` still need migration. |
| 5 | Regression testing | ⚙️ Rolling | SDL3 build + smoke runs executed after each phase; need focused dumps/spoiler verification once file helpers settle. |

### Next Implementation Targets
1. **Stage S7 (in progress):** With `path_parse()`, `path_build()`, `path_temp()`, and the `fd_*` helpers now returning `bool`, audit the remaining loaders (`init1.c`, `init2.c` follow-ups, `squelch.c`, metarun maintenance) to use the new error contract.  
2. **Stage S7:** Finish the Phase 4 sweep by moving the remaining bespoke file walkers out of `init1.c`, `init2.c`, and `squelch.c` onto `fs/*`.  
3. **Stage S9 prep:** Re-test character dumps, spoilers, and metarun backups after the filesystem changes; capture evidence in `session_notes.md`.  
4. Track the terminal (`z-term`) refactor separately—keep that file stable until the dedicated plan lands.

---

## Phase 1 – Fix Highscore Bug (Complete)
- [x] `highscore_add()` always flushes via `SDL_FlushIO()` and has no nested `#ifdef`.  
- [x] `open_scores_file_versioned()` is SDL-only.  
- [x] Manual test: enter multiple scores, quit, and confirm all entries persist.

## Phase 2 – Remove `USE_SDL` Conditionals (Complete)
### `files.c`
- [x] Removed all `#ifdef USE_SDL` branches; SDL path is the only implementation.  
- [x] Deleted `SCORE_FILE_TYPE`, `SCORE_FILE_CLOSE`, and `CHAR_FILE_PRINTF` macros in favor of SDL helpers.

### `util.c`
- [x] Removed `my_fopen/my_fclose/my_fgets/my_fputs` and every `fd_*` shim.  
- [x] Only the new `sdl_*` helpers remain.

### `wizard1.c`, `birth.c`, `angband.h`
- [x] Purged SPOIL macros and `USE_SDL` guards.  
- [x] `text_out_file`/`highscore_fd` declarations are always SDL-based.

## Phase 3 – Modernize to C17 (Ongoing)
- [x] Formatting/logging layer moved into `format.c`/`format.h` (Phase 3 milestone).  
- [ ] Replace residual `errr` return types with `bool` where practical (tracked via `fs/path.*` and loader callers).  
- [ ] Audit size/length code for `size_t` usage.  
- [ ] Ensure errno/SDL errors bubble back to callers for better diagnostics.  
- [ ] Tighten `const` correctness + drop redundant casts during rewrites.

## Phase 4 – Filesystem + Loader Sweep (Ongoing)
- [x] SDL I/O + path helpers live under `src/fs/`.  
- [x] `util.c` trimmed down to gameplay utilities.  
- [ ] `init1.c`/`init2.c`: still contain ad-hoc file walkers—migrate to `fs/*`.  
- [ ] `squelch.c` + any lingering dump generators: ensure they use the new helpers.  
- [ ] Remove final dependencies on proprietary `errr`/`fd_*` patterns once above migrations finish.

## Phase 5 – Testing Matrix
- [x] Save/load smoke test (2025-11-09 run).  
- [x] Highscores: add >1 entry and re-open menu.  
- [ ] Character dumps + spoilers after filesystem sweep.  
- [ ] Metarun/scores backup verification post-migration.

## Out-of-Scope
- `src/z-term.c` refactor: moved to a dedicated plan. Keep the current terminal abstraction untouched while SDL panes remain the rendering path.
