# Global Variable Localization Plan

## Objectives
- Reduce reliance on `extern` globals declared in `src/externs.h` by confining state to the subsystems that own it.
- Prepare the codebase for modern C17 practices (deterministic dependencies, testable modules, and thread-friendly state).
- Provide actionable work items that can be implemented incrementally without breaking existing builds.

## Candidate Refactors

### 1. Input subsystem toggles (`src/variable.c:98-107`, `src/externs.h:97-103`)
**Current state:** `inkey_base`, `inkey_xtra`, `inkey_scan`, `inkey_flag`, and `hide_cursor` are global booleans manipulated exclusively from the input helpers in `src/util.c:1145-1385` and from two call sites in `src/cmd4.c:10053-10065`. They represent transient state for the keyboard loop.

**Issues:** The globals leak internal `inkey()` implementation details through `externs.h`, complicating unit testing and encouraging accidental writes.

**Plan:**
1. Create a `static struct input_state` inside `src/util.c` that houses the five flags.
2. Replace direct assignments in `cmd4.c` with small helper functions such as `inkey_set_base(bool)`, declared in `externs.h`.
3. Remove the global declarations from `externs.h`, keeping the entire state private to `util.c`.

**Validation:** Run the SDL3 build (`build-cmake.bat`) and exercise repeat-key scenarios plus the in-game text prompt to ensure `inkey_base` flows still work.

### 2. Mini screenshot buffers (`src/variable.c:136-138`, `src/files.c:6033-7531`)
**Current state:** `mini_screenshot_char[7][7]` and `mini_screenshot_attr[7][7]` live in `variable.c` even though only the functions in `src/files.c` (`mini_screenshot()` and `prt_mini_screenshot()`) touch them.

**Issues:** Keeping rendering scratch buffers global makes any display refactor harder and adds unnecessary surface area for save/load code.

**Plan:**
1. Move the arrays (and any related constants) into `files.c`, marking them `static`.
2. Keep the public API limited to the two existing functions so callers (`src/dungeon.c:3913`) do not need access to the buffers.
3. Delete the `extern` declarations to prevent new dependencies.

**Validation:** Trigger mini-screenshots before and after a build to confirm dump and overlay rendering still works (see `files.c:7526-7531` for expected behavior).

### 3. Projectile path ignore toggles (`src/variable.c:672-674`)
**Current state:** `project_path_ignore` and its `*_x/_y` coordinates are globals that help `melee2.c:1597-1655` suppress a monster square when tracing a bolt in `cave.c:5428-5550`.

**Issues:** The variables represent per-call temporary state yet remain mutable globals, so concurrent calls or early exits could leave stale coordinates and create hard-to-trace bugs.

**Plan:**
1. Extend `project_path()` (declared in `src/externs.h:214`) to accept an optional context struct, e.g., `const project_path_mask* ignore`.
2. Update `melee2.c` to construct the struct on the stack and pass it down instead of toggling globals.
3. Remove the global variables and adapt `projectable()` / `update_flow()` helpers to read from the new context (defaulting to `NULL` to preserve existing callers).

**Validation:** Rebuild and run regression tests that cover monster breath attacks targeting adjacent squares; confirm there are no regressions in path carving or in the AI’s decision making.

### 4. Command-line configuration flags (`src/variable.c:52-59`)
**Current state:** `arg_sound`, `arg_graphics`, `arg_force_original`, and `arg_force_roguelike` are globals set in `src/main.c:334-356` and read in `src/dungeon.c:3713-3715`, `src/main-sdl.c:436, 1388-1393`, and `src/xtra1.c:4262`.

**Issues:** Startup configuration leaks across unrelated modules, making it difficult to add new front-ends or unit tests without instantiating the entire runtime.

**Plan:**
1. Define a `struct runtime_cli_args` in `main.c` and populate it while parsing `argv`.
2. Thread that struct through to the frontend bootstrap (e.g., extend the `GameOptions` or `game_startup_state` that already flows into `main-sdl.c`), storing it in the existing SDL `g_state` rather than globals.
3. Update call sites to inspect the struct or derived configuration instead of touching globals, then delete the `extern` declarations.

**Validation:** Build and run both ASCII and SDL3 front-ends, ensuring that `-r`/`-o` keyset switches, graphics presets, and sound toggles still take effect.

### 5. Background color toggle (`src/variable.c:139`)
**Current state:** `use_background_colors` is a global read in `src/cave.c:1044` and `src/cave.c:1818` to determine how to compose tile colors.

**Issues:** Rendering policy should belong to the UI/theme subsystem (e.g., `src/ui/colors.c`), not the dungeon module. The current arrangement prevents testing alternative palettes in isolation.

**Plan:**
1. Add a field to the existing UI color/theme config (see `src/ui/colors.c` and `src/ui/colors.h`) to hold the background-color policy.
2. Make `cave.c` query the theme via a small accessor (e.g., `ui_colors_use_backgrounds()`), keeping the state encapsulated.
3. Remove the global flag once the accessor is wired in.

**Validation:** Cycle through ASCII, pseudo-graphical, and tiles modes to ensure walls/floors honor the background color preference.

## Rollout Strategy
- Tackle one subsystem at a time, landing each change with focused builds/tests to minimize merge risk.
- After each refactor, regenerate `externs.h` (if automated) and run `build-cmake.bat` to confirm SDL3 binaries still compile.
- Record any follow-up todos (e.g., additional globals discovered) in `session_notes.md` to keep this plan current.

