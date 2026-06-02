# Two-page book + page-curl turn animation for the race-selection screen

## Context

The first character-selection screen (race/"The War of the Jewels") is rendered
in SDL "book mode" as one tall page: a light-blue trial intro, the white war
chronicle, a light-blue charge ("Choose now whose name you will wear..."), the
selectable peoples list, then the highlighted people's lore. The screen is meant
to *symbolize a book*, but it reads as a single scroll.

Goal: split this content across **two book pages** and play a **page-curl/peel
animation** when turning between them.

Decisions confirmed with the user:
- **Split:** Page 1 = trial intro (light blue) + war chronicle (white). Page 2 =
  the charge (light blue) + peoples list + lore. Each page begins with a
  light-blue block.
- **Navigation:** Left / Right arrows, Space, plus on-screen buttons at the
  bottom for mouse. (Left already = "back a step"; Right/Space = forward.)
- **Animation:** page-curl / peel.

## Relevant code

- Book render block: [main-sdl.c:10400-10571](src/main-sdl.c#L10400-L10571)
  (`sdl_character_sheet_screen_render`). Draws frame_top (L_BLUE) / intro (white)
  / frame_bottom (L_BLUE) / list / lore.
- Screen state struct: [main-sdl.c:847-876](src/main-sdl.c#L847) — holds
  `select_frame_top/intro/frame_bottom`, `select_rows`, `select_book_mode`, etc.
- Per-iteration rebuild that wipes `select_*`:
  [main-sdl.c:10997-11018](src/main-sdl.c#L10997) (`..._begin_select`). New
  page/anim fields must NOT be cleared here.
- Input loop (shared terminal/SDL state machine):
  [birth.c:1938-2490](src/birth.c#L1938). Book mode = `page->intro != NULL`
  (`book` bool at [birth.c:2004](src/birth.c#L2004)). Left `'4'` = back
  ([birth.c:2349](src/birth.c#L2349)); confirm incl. Space
  ([birth.c:1012](src/birth.c#L1012), [birth.c:2357](src/birth.c#L2357)).
- Page text source: [birth.c:3384-3417](src/birth.c#L3384)
  (`birth_frame_top` / `birth_intro_lore` / `birth_frame_bottom`); wired in
  `get_player_race` [birth.c:3669-3678](src/birth.c#L3669).
- Timed-render hook merge block: [main-sdl.c:35790+](src/main-sdl.c#L35790)
  (`callback_sdl_xtra` / `TERM_XTRA_EVENT`), pattern from
  `sdl_gamepad_pending_timeout_ms` [main-sdl.c:29671](src/main-sdl.c#L29671).
- Reusable helpers: `sdl_char_sheet_draw_wrapped`
  ([main-sdl.c:9586](src/main-sdl.c#L9586)), `sdl_char_sheet_draw_history`,
  `sdl_char_sheet_wrap_text`, `sdl_char_sheet_line_h`, `sdl_story_font_for_height`,
  `sdl_char_sheet_add_hit`, `sdl_char_sheet_draw_focus_rect`,
  `sdl_char_sheet_choice_focused`.

## Implementation

### 1. State (main-sdl.c, struct at ~847)
Add fields that survive `begin_select`:
- `int select_page;` (0 or 1)
- `int select_page_count;` (1 for non-book; 2 for book mode)
- page-turn animation: `bool page_turn_active; Uint64 page_turn_start_ns;
  int page_turn_dir; (+1 fwd / -1 back)` and two target textures
  `SDL_Texture* page_turn_from_tex; SDL_Texture* page_turn_to_tex;` plus their
  pixel size.

In `begin_select` ([main-sdl.c:11002](src/main-sdl.c#L11002)) leave `select_page`
untouched (like `sheet_scroll` is preserved when context already SELECT). Reset
`select_page = 0` and free any turn textures only when entering the screen fresh
— detect via the existing `context != SDL_CHARACTER_SHEET_BIRTH_SELECT` guard at
[main-sdl.c:10997](src/main-sdl.c#L10997).

### 2. Refactor book render into a per-page helper (main-sdl.c)
Extract the body of the book block ([main-sdl.c:10400-10566](src/main-sdl.c#L10400))
into `static void sdl_char_sheet_render_book_page(int page, SDL_FRect region, bool
register_hits)` that draws into the **current render target**:
- Compute one shared `body_px` font size that fits the *taller* of the two pages
  (measure page-1 sections and page-2 sections, take the smaller fitting size) so
  text size stays constant across the turn — keeps the "same book" feel.
- `page == 0`: draw frame_top (L_BLUE) + intro (white), vertically centered.
- `page == 1`: draw frame_bottom (L_BLUE) + the grouped selectable list + lore
  (the existing list/lore code). Only register people-row click hits when
  `register_hits` is true and `page == 1`.

### 3. Page-turn animation (main-sdl.c)
- New module functions:
  - `void sdl_select_begin_page_turn(int dir)` — if book mode and a turn is not
    active and the target page is in range: render the current page and the
    destination page each into an offscreen `SDL_Texture`
    (`SDL_CreateTexture(..., SDL_TEXTUREACCESS_TARGET, ...)`, `SDL_SetRenderTarget`,
    call the page helper with `register_hits=false`, restore target), set
    `select_page` to the destination, and start the timer.
  - `int sdl_select_page(void)` getter and `int sdl_select_page_count(void)` for
    birth.c.
  - `static int sdl_select_page_turn_timeout_ms(Uint64 now)` returning ~16ms
    while active, else -1; add it into the timeout merge in `callback_sdl_xtra`
    ([main-sdl.c:35790+](src/main-sdl.c#L35790)) so `inkey` keeps waking to draw
    frames.
- In the book branch of `sdl_character_sheet_screen_render`:
  - If `page_turn_active`: compute `t = elapsed / DURATION` (~450ms, ease-out).
    Draw the destination texture flat as the underlying page, then draw the
    outgoing texture **peeling** on top via `SDL_RenderGeometry`: map the page
    texture onto an NxM vertex grid whose right portion curls around a shrinking
    cylinder from the right/corner toward the spine, with a dark→light shadow
    gradient on the curled strip (vertex colors) to fake paper. Forward turn
    peels the old page away to reveal page 2; back turn reverses. When
    `t >= 1`, clear `page_turn_active`, free textures, request a normal redraw.
  - Else: call `sdl_char_sheet_render_book_page(select_page, region, true)`.
  - Bottom buttons: in the prompt area draw click targets — page 0:
    `[ Turn the page \xe2\x80\xba ]`; page 1: `[ \xe2\x80\xb9 Previous page ]` — registered
    with `sdl_char_sheet_add_hit` using new negative choice ids
    (`SDL_SELECT_CLICK_PAGE_NEXT`, `SDL_SELECT_CLICK_PAGE_PREV`). Suppress
    buttons + people hits while a turn is animating.
- Fallback: if a target texture can't be created (older driver), skip geometry
  and just swap pages instantly (no crash). `SDL_RenderGeometry` is SDL3 core, so
  this is a safety net, not the expected path.

### 4. Input wiring (birth.c, book branch of `get_player_choice`)
Only when `book` is true and a turn is not animating:
- **Forward** (page 0 -> 1): Right `'6'`, Space, or `SDL_SELECT_CLICK_PAGE_NEXT`
  click -> `sdl_select_begin_page_turn(+1)`, `continue` (do NOT confirm/select).
- **Back**: Left `'4'` / `SDL_SELECT_CLICK_PAGE_PREV`:
  - page 1 -> turn back to page 0 (`begin_page_turn(-1)`), `continue`.
  - page 0 -> existing behavior: return `INVALID_CHOICE` (back to main menu).
- Confirm (Enter / Space / `'6'`) and people clicks only act on **page 1**. On
  page 0, route Space/Right to the forward turn (guard the existing
  `birth_confirm_input` / `'6'` block at [birth.c:2357](src/birth.c#L2357) and
  the ESC/`'4'` block at [birth.c:2349](src/birth.c#L2349) with the page check).
- While `sdl_select_page() == 0`, ignore Up/Down list movement (no visible list);
  on page 1 keep existing list navigation.
- New click ids handled in the SDL click switch near
  [birth.c:2317](src/birth.c#L2317).
- Reset to page 0 once at the start of `get_player_race`
  ([birth.c:3634](src/birth.c#L3634)) via a small
  `sdl_character_sheet_screen_reset_select_page()` call, so re-entering the screen
  always opens on page 1 of the book.

### 5. externs.h
Declare the new public functions used by birth.c
(`sdl_select_begin_page_turn`, `sdl_select_page`, `sdl_select_page_count`,
`sdl_character_sheet_screen_reset_select_page`) alongside the existing
`sdl_character_sheet_screen_set_select_*` declarations.

## Notes / risks
- The peel is the only visually risky part. Vertex-grid cylinder warp +
  per-vertex shadow is the standard 2D fake; if it looks off, the easing/shadow
  are tunable constants. Texture sizing should match the on-screen region in
  pixels (account for canvas scale) so the warped page stays crisp.
- Terminal (non-SDL) fallback path is unchanged: it still shows the one-page
  text. Two-page split is SDL-only (book mode is SDL-only already).

## Verification
1. Build the SDL target and launch (see the project run skill / existing build).
2. New game -> character creation -> first screen ("The War of the Jewels").
3. Confirm page 1 shows trial intro + chronicle only, with a bottom
   "Turn the page" button; no peoples list visible.
4. Press Right / Space / click the button -> page-curl animation plays, revealing
   page 2 (charge + peoples list + lore).
5. On page 2: Up/Down move the highlight, lore updates, Enter/Space/click selects
   a people and proceeds. Left / "Previous page" curls back to page 1.
6. On page 1, Left / Esc exits to the main menu (unchanged).
7. Resize the window and repeat to confirm font sizing stays consistent across
   both pages and the animation scales.
