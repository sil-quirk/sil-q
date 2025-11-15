# Key Handling Mechanics Audit

This note summarizes how keyboard input currently flows through Sil-QH and highlights opportunities to modernize the system.

## 1. Input Pipeline Overview
- **Platform layer ➜ term queue**: Each `main-*.c` backend translates SDL/OS events into `Term_keypress()` calls, which enqueue raw `char` codes in the term buffer managed in `src/z-term.c:126-2050` (notably `Term_keypress()` at `src/z-term.c:1960` and `Term_inkey()` at `src/z-term.c:2015`).
- **Macro interpreter**: `inkey_aux()` in `src/util.c:1635-1770` peeks into the queue, waits up to 500 ms between characters, and matches prefixes against the macro table. When a pattern matches, its action string is pushed back onto the queue, delimited by ASCII 30 to prevent nesting.
- **inkey() gatekeeper**: `inkey()` (`src/util.c:1841-2044`) enforces the `inkey_*` flags (flush, scan, base, flag), normalizes Escape/backquote, consumes macro delimiters, and feeds characters to higher layers. It also honors the `inkey_next` pointer so keymaps can inject synthetic sequences without triggering macro expansion.
- **Command acquisition**: `request_command()` (`src/util.c:4660-4904`) loops until it can set `p_ptr->command_cmd`, `command_arg`, and `command_dir`. It handles auto-commands, counts, bypass prefixes, and keymaps before surfacing a command to the gameplay code.

## 2. Macro & Keymap Data Structures
- **Macros**: Stored as pattern/action string pairs in the `macro__pat`/`macro__act` arrays with helper indices in `macro__use` (`src/util.c:1311-1493`). Pref files use `P:<trigger>` (`src/files.c:521-529`) to register actions and `T:` records (`src/files.c:573-666`) to describe human-readable trigger names, modifier metadata, and alternate shifted keycodes.
- **Keymaps**: `keymap_act[KEYMAP_MODES][256]` holds per-mode action strings indexed by a single byte key (`src/variable.c:494-497`, `src/defines.h:1025-1033`). Pref commands `C:<mode>:<key>` load data (`src/files.c:531-555`), and the in-game macro menu in `src/cmd4.c:9795-10064` lets players query, create, or remove keymaps.
- **Modes**: `request_command()` chooses a mode based on `angband_keyset` and `hjkl_movement` (`src/util.c:4671-4678`), so each physical key can dispatch different action strings depending on the keyset combination.

## 3. Prefixes, Counts, and Special Keys
- **Repeat counts**: Pressing `R` (Sil sets) or `0` (Angband sets) enters a numeric prompt that fills `p_ptr->command_arg`, defaulting to 99 if left at zero and restoring the previous arg when blank input is given (`src/util.c:4721-4815`). After a command consumes the count it usually seeds `p_ptr->command_rep` (e.g., `src/cmd2.c:676`).
- **Bypassing keymaps**: A leading backslash fetches the next literal command via `get_com()` and explicitly sets `inkey_next` to an empty string so no keymap translation occurs (`src/util.c:4818-4827`).
- **Control-prefix**: A leading caret prompts for another character and passes it through the `KTRL()` macro to manufacture a control code (`src/util.c:4829-4835`), enabling sequences like `^tt` to send `CTRL-T`.
- **Macro delimiters**: ASCII 30 (`^^`) marks the end of a macro action, ASCII 31 (Ctrl-underscore) toggles the “under” state that strips all keys until a control char arrives, and ASCII 29 serves as the placeholder for “magic” subcommands (`src/util.c:1649-2030`).
- **Automatic conversions**: Backquote is mapped to Escape after macro handling (`src/util.c:1992-1994`), letting machines without Escape still trigger menus.
- **Command queueing**: When `act = keymap_act[...]` yields a string, it is copied into `request_command_buffer` and handed to `inkey_next` so the expanded sequence re-enters the `inkey()` path without recursive keymap lookup (`src/util.c:4837-4852`).

## 4. Configuration Touchpoints
- **Pref-driven setup**: The loader accepts macro (`P`), keymap (`C`), and trigger (`T`) tokens in any preference file, meaning `lib/pref/*.prf` can define per-platform bindings (`src/files.c:521-666`). Dumping keymaps uses `keymap_dump()` to write the active mode to disk (`src/cmd4.c:9633-9710`).
- **Session toggles**: Players flip between Sil/Angband and hjkl cursor sets via option bits, but those booleans only pick which of the four keymap tables to consult (`src/util.c:4671-4678`). No runtime context (e.g., inventory vs. targeting) is tracked.
- **Macros vs. keymaps**: Macros can target side effects such as `\\[F1]` -> command sequences thanks to trigger templates, whereas keymaps are limited to single-byte triggers but inject multi-character command strings; both ultimately funnel through the same `inkey_next` buffer.

## 5. Limitations in the Current Approach
- **ASCII-first assumptions**: Keys are treated as single `char` values everywhere (`keymap_act[...][256]`, `Term_keypress(int)`), which makes it hard to assign bindings to scancodes, international layouts, or simultaneous modifier chords beyond what the macro template encodes.
- **Macro timing heuristic**: `inkey_aux()` busy-waits in 10 ms increments, up to 500 ms total, to decide whether a keypress could still extend a macro trigger (`src/util.c:1676-1710`). This ties responsiveness to polling delays and can drop partly typed triggers when the OS stalls.
- **Control character sentinels**: Reliance on ASCII 29–31 for macro delimiters and meta states assumes those control codes are never part of input, which is brittle if we ever want to map such characters legitimately or handle IME/Unicode input.
- **Bypass prefixes leak UI concepts**: `\` and `^` prompts break flow by requiring blocking `get_com()` calls on the main message line, and the command-count prompt temporarily hijacks row 0, leading to redraw churn (see `src/util.c:4729-4804`).
- **Context-free keymaps**: Only four static tables exist (Sil/Sil+HJKL/Angband/Angband+HJKL). We cannot express bindings that differ between targeting, inventory, or text input modes without ad hoc conditionals in every consumer.
- **Pref syntax friction**: Creating macros or keymaps requires memorizing encoded control sequences (e.g., `\[\[ctrl-F2]]` → `^_O_64\r`). The macro template/conflict logic in `trigger_text_to_ascii()` is difficult to reason about and lacks validation feedback (`src/util.c:855-1116`).
- **No SDL-level features**: SDL 3 sends high-level `SDL_KeyboardEvent`s with physical scancodes, modifiers, repeat info, and text input events, but the main-SDL driver collapses all of that down to a single byte before it reaches the game, preventing richer input handling (e.g., distinguishing left/right modifiers or non-Latin text).

## 6. Modernization Opportunities
1. **Adopt structured key events**: Introduce an intermediate `game_key_event` struct (scancode, mods, text, repeat) populated in `main-sdl.c` and stored in a richer queue (e.g., replacing `Term_keypress(int)` with something typed). Then update `inkey()`/`request_command()` to operate on symbolic actions rather than raw chars. This would remove the ASCII-only constraint and allow platform-specific handling (source touchpoints: `src/main-sdl.c`, `src/z-term.c`, `src/util.c`).
2. **Unify macros and keymaps**: Replace the dual system with a JSON/TOML configuration that maps *contexts + trigger chords* to *action pipelines*. Each entry could optionally reference reusable “actions” (commands, menus, cursor moves). Persisting to disk would become data-driven, removing the need for the `P`, `C`, and `T` directives and the 500 ms macro timeout logic.
3. **Context-aware binding layers**: Add a binding resolver that selects different maps for global play, targeting, text prompts, stores, etc., based on a lightweight context enum. This isolates menu-specific overrides from dungeon movement and would reduce scattered `angband_keyset` checks (refactor around `request_command()` and the `cmd?_command_*` call sites).
4. **Declarative repeat & modifier prefixes**: Instead of abusing `R`/`0`, treat numeric prefixes as part of the binding grammar (`digit* <command>`). Build a small parser that consumes digits before dispatching and display the pending count in a dedicated UI widget, avoiding direct text prompts on row 0.
5. **Macro recorder rework**: Record and replay high-level commands (e.g., “apply `cmd1_do_cmd_attack`”) rather than literal bytes. This would survive binding changes and simplify the escape characters/reserved control codes currently required to delimit macros.
6. **Improved configuration UX**: Build an in-game configuration screen (SDL overlay) that listens for the next key chord, displays its scancode/modifier tuple, and lets the user assign actions, mirroring modern roguelikes. Persisting that data can reuse the same serialization format proposed above instead of the bespoke `cmd4.c` menu.
7. **Testing & tracing hooks**: Leverage `log_trace` when key events are dropped or remapped (e.g., when the macro detector times out) so we can debug binding issues more easily, and add regression tests covering prefix handling using scripted `Term_keypress()` injections.

Taken together, these changes would move Sil-QH toward a more modern, flexible input system that respects SDL’s capabilities, improves accessibility (better international keyboard support), and simplifies both configuration and maintenance.
