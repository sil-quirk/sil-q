---
applyTo: '**'
---

# Sil-More Project Memory (Concise)

## Purpose
- Serve as the single quick-reference memory for agents working on Sil-More (Sil-Q variant).
- Track only living project knowledge: current workflows, stable systems, UX preferences, and watchpoints. Update or prune aggressively.

## Project Snapshot
- C roguelike codebase under `src/`; assets/config live in `lib/` and assorted INI files.
- Fork emphasises modern UX upgrades: combat history, unified look command, enhanced inventory/equipment menus, improved window/fullscreen handling.

## Build & Run
- Primary target: Windows build via Cygwin toolchain.
- Build: `make -f Makefile.cyg -j8`
- Run latest build: `make -f Makefile.cyg launch`
- Ensure the Cygwin login shell (`C:\Soft\Cygwin\bin\bash.exe`) keeps `/usr/bin:/bin` at the front of PATH (VS Code settings already do this).

## Environment Notes
- mingw32 toolchain bundled; no extra package manager required.
- Game stores executable in repo root (`Sil-More.exe`) plus INI profiles (`sil.INI`, `silW.ini`, `silF.ini`, `sil_steamdeck.INI`).
- Keep window style changes consistent with automatic INI persistence when testing fullscreen/subwindow work.

## Key Systems (Status 2025-09)
- **Combat History Menu** – Production-ready. Main menu entry logs 100 rounds with exact combat roll formatting and color.
- **Inventory/Equipment Menus** – Scrollable overlay with arrow navigation, floor item support (`-)` labels via `-` key), Steam Deck-aware letter handling, and command cycling (`u` / `x`).
- **Unified Look Interface** – Single `l` command combining look/locate/monster/object views. Tab (or `i/e` on Steam Deck) cycles entities, Shift+arrows pan panels, blue cursor highlights, banners auto-clear, viewport restored on exit.
- **Combat Roll Overlay** – Optional 0–3 line main-terminal display. Lines start at `COL_MAP`, fixed 65-char clear width; default hides until combat occurs.
- **Windowing & Fullscreen** – Safe transitions across normal/borderless/minimal subwindow styles with robust error handling and state restoration.

## UX & Gameplay Preferences
- Respect column calculations from `show_inven()` / `show_equip()`; never hard-code highlight positions.
- Floor items must retain `-)` prefix and respond to the `-` shortcut in enhanced menus.
- When `STEAMDECK_SUPPORT` is enabled, treat Space as confirmation on stair/shaft prompts; otherwise require explicit `y/n`.
- Keep auto drop-down lists enabled by default (`OPT_auto_display_lists = true`).
- Maintain 65-character clearing width for combat roll lines unless UI layout changes.

## Operational Guidelines
- Add knowledge under the appropriate heading; avoid duplicating history already represented in code or commits.
- Summaries over timelines: capture the current state, not the development journey.
- Include open risks or regressions under Watchpoints and remove them once resolved.
- Do not store secrets or user-identifiable data.

## Watchpoints
- UI changes often impact highlight alignment and window styles—double-check these after modifications.
- When touching unified look or inventory subsystems, monitor for regressions in panel scrolling and banner clearing.

## Last Verified
- Compiled September 26, 2025 based on latest repository state and recent project notes.
