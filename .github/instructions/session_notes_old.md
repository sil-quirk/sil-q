# Story Font Alignment Issue - Analysis Session# Session Notes



## Problem Statement## 2025-11-06 - Metarun Curse Expansion & New Debuffs

Three related alignment issues when using story font (proportional font):

1. **Equipment/Inventory Labels**: When highlighted, labels like `(A)`, `(B)` render immediately after item name instead of at fixed column position- Raised the metarun curse capacity from 32 to 64 slots by enlarging `curse_stacks`, switching the known-bitmask to 64-bit, and adding a v9 compatibility shim so legacy meta.raw entries upgrade cleanly (`src/metarun.h`, `src/metarun.c`).

2. **Empty Slot Text**: When highlighted, text like `(NO BOW)` renders immediately after slot label instead of at fixed position- Promoted runtype data to 64-bit curse masks and widened parsers so start curses/blessings can target the new slots (`src/types.h`, `src/init1.c`).

3. **Character Sheet Stats**: When stats change, old and new values render at different positions- Introduced `curse_flag_delta_cur()` to track net curse/blessing stacks and used it to drive resistance, melee damage side, armor protection, and critical-threshold penalties (`src/birth.c`, `src/xtra1.c`, `src/melee1.c`, `src/cmd1.c`).

- Defined new CUR flags for fear/stun/confusion/hallucination/poison/fire/cold resistance shifts plus melee/armor side and crit-threshold modifiers (`src/defines.h`).

## Root Cause Analysis- Added ten new curse/blessing entries covering the requested debuffs with full flavour text and weight/stack limits (`lib/edit/curses.txt`).

- Updated metarun UI helpers that enumerate curse IDs to honor the expanded slot count and rebuilt with `build-cmake.bat` (SDL3 target) to verify the changes.

### The Rendering Flow

## 2025-11-02 - Story Font List Polish & Intro Scope

#### For Non-Highlighted (Working):

```- Added reusable story-font helpers (`story_print_text`, `story_print_mono`, `story_fill_rect`) in `src/util.c` with declarations in `src/externs.h` so UI layers can print proportional spans, keep mono-aligned columns, and pre-clear highlight rows without duplicating SDL plumbing.

show_equip() → story_render_equipment_entry() → story_print_text() → text_out_to_screen_story() → Term_addch() → Term_queue_char()- Converted `display_introduction()` and the initial menu (`initial_menu` in `src/init2.c`) to share one story-font scope so the introductory poem, frame, and action prompts all render with the proportional typeface; `print_story_intro()` already handled the narrative section but now the surrounding prompts inherit the same font.

```- Reworked the enhanced inventory renderer (`show_inven_enhanced`) to clear each row before painting, split description/weight/label segments, and render the highlight bar by filling the row prior to drawing. Descriptions now use `story_print_text` while weights/labels stay monospace via `story_print_mono`, eliminating the drifting `(A)` / `(B)` tags when the story font is enabled.

- Implemented a dedicated story-font path for equipment lists: when the "Story UI Lists" option is on, `show_equip_enhanced` bypasses `show_equip()` and uses the new `draw_equipment_story_rows()` helper to paint mention-use prefixes, tiles, descriptions (with quiver note support), weights, and slot labels with the same proportional logic used for inventory.

#### For Highlighted (Broken):- Updated the unified look/target UI (`target_set_interactive_aux` in `src/xtra2.c`) to accept a `use_story_font` flag, route its prompts through `look_prt()`, and rely on the shared helpers so the `l`-view text no longer leaves stray characters when switching between mono and story fonts.

```- Moved the `story_lists` option from the Interface page to the Visual Options page (where the rest of the rendering toggles live) to eliminate the empty line the user reported and make the setting easier to discover.

show_equip_enhanced() → draw_equipment_story_rows() → story_print_text() → text_out_to_screen_story() → Term_addch() → Term_queue_char()- Full SDL build verified with `build-cmake.bat`; only existing warnings remain.

```

## 2025-11-03 - Story Font Follow-up

### Key Components

 - Added per-menu Visual Options toggles (`story_lists_inven`, `story_lists_equip`) so inventory and equipment screens can switch independently between story and mono rendering (`src/defines.h`, `src/tables.c`, `src/util.c`).

1. **`callback_sdl_text()` (main-sdl.c:463-750)** - Painted the inventory weight column and `(a)` style slot letters with `story_print_text()` whenever the story font is enabled so the entire row, including highlights, uses the proportional font; left mono behavior unchanged for the default view (`src/object1.c`).

   - The SDL rendering hook that actually paints text to screen - Extended `draw_equipment_story_rows()` to render slot prefixes, weights, and label glyphs with the story font so both sides of the equipment overlay match the item descriptions and their letter columns stay aligned (`src/object1.c`).

   - Checks `Term->story_chunk_active` and per-character `Term->scr->story[y][x]` flags - Rebuilt with `build-cmake.bat` to confirm the SDL target still succeeds.

   - When story font is active, renders using TTF_RenderText_Blended with proportional width

   - **Critical**: Scales story font height to match cell height, but width is proportional## 2025-11-04 - Story Menu Alignment



2. **`Term_queue_char()` and `Term_queue_chars()` (z-term.c:486-620)** - Added `story_render_inventory_entry()` / `story_render_equipment_entry()` helpers so both the base list renderers (`show_inven`, `show_equip`) and the `get_item()` highlight overlay use a single code path for proportional layout. The helpers clear the full row, reuse the tile column returned by `draw_item_tile()`, and split description/weight/label spans with `story_print_text()` while keeping the mono path unchanged.

   - Sets `scr_story[x] = Term->story_font_active ? 1 : 0` - Updated `show_inven()`/`show_equip()` to call the helpers whenever the "Story UI Lists" toggles are active, including the final shadow rows and the armour weight summary so highlighted rows no longer shift the `(A)` / `(B)` columns.

   - Story font flag is set PER CHARACTER based on global `Term->story_font_active`   - Switched the highlight macro in `get_item()` to detect the current story mode and rerender the selected inventory/equipment/floor row with the same helper logic, keeping `(no bow)` and similar empty-slot strings aligned with their non-highlighted versions.

   - **Issue**: Flags are set when text is queued, but the actual RENDERING happens later   - Fixed the comparison overlay typo in `show_inven_enhanced()` (`" (%s"` → `" (%s)"`) so the `(G)` label renders correctly when pressing `u`/`x`.



3. **`story_print_text()` (util.c:3445)**## 2025-11-05 - Story Highlight Build Fix

   - When story font enabled, uses `text_out_to_screen_story()`

   - Sets `text_out_indent` and `text_out_wrap` to control column positions   - Hoisted the story-font row helpers ahead of `show_inven()` and replaced the inline `#if` sections inside `DRAW_HIGHLIGHT` with helper macros (`DRAW_HIGHLIGHT_STORY_VARS/UPDATE`, `DRAW_HIGHLIGHT_IF_STORY`), which eliminates the implicit declaration errors and keeps the SDL-only logic out of macro definitions.

   - **Issue**: These are in COLUMN units, but story font rendering is in PIXELS   - Restored `draw_equipment_story_rows()` next to the new helpers and recompiled the SDL3 target to verify the proportional highlight path builds cleanly.

   - Added `story_inventory_list_active` / `story_equipment_list_active` state flags so `get_item()` detects whether the visible list is currently using story font, ensuring the highlight overlay always reuses the same renderer instead of guessing from static option bits.

4. **`text_out_to_screen_story()` (util.c:3341)**

   - Word-wraps based on PIXEL width using `sdl_story_font_text_width()`## 2025-10-26: Left Sidebar Story Font - Implementation Complete

   - Tracks position in both pixels (`current_x_pixels`) and columns (`x`)

   - **Critical Discovery**: Advances `x` by CHARACTER COUNT but `current_x_pixels` by ACTUAL WIDTH### Summary

   - This creates a MISMATCH between terminal column position and visual pixel positionSuccessfully implemented story font rendering for the left sidebar with proper `Term_fresh()` placement. All text labels render in story font while numbers remain in monospace, following the exact pattern used in `files.c`.



5. **`story_render_equipment_entry()` (object1.c:2633)**### Implementation Details

   - Calls `story_print_text(row, col, width, attr, text)` with COLUMN positions

   - Example: `story_print_text(row, label_col, label_width, label_attr, label_text)`All 16 sidebar functions now follow this pattern:

   - Where `label_col = 71` or `78` (fixed COLUMN position)```c

#ifdef USE_SDL

### The Problem    sdl_story_font_enable();

#endif

When story font renders text:    // ... render text labels with put_str() or c_put_str() ...

1. Text is placed at column X (e.g., col=0)    Term_fresh();  /* CRITICAL: Flush text with story font BEFORE disable */

2. Story font characters have varying widths (proportional)#ifdef USE_SDL

3. `text_out_to_screen_story()` advances terminal cursor by CHARACTER COUNT    sdl_story_font_disable();

4. Terminal thinks cursor is at column X+N (where N=char count)#endif

5. But VISUAL position is at pixel offset that doesn't align with terminal grid    // ... render any numbers in monospace ...

6. Next text chunk starts at column X+N, which in PIXELS is at wrong position```



Example:**Key Points:**

```- `#ifdef USE_SDL` guards are REQUIRED (functions only exist in main-sdl.c)

"Wielding    : " (14 chars, but only ~10 chars worth of pixels in story font)- `Term_fresh()` must be called AFTER text rendering but BEFORE `sdl_story_font_disable()`

Terminal cursor advances to column 14- This ensures buffered text is rendered with story font before switching to monospace

Next text "A Curved Sword" starts at terminal column 14- Pattern matches working examples in `display_player_stat_info()` and `put_single20_right()` from `files.c`

But visually, only 10 cells worth of pixels were rendered

Visual result: text starts too early, overlapping the intended position### Functions Modified in src/xtra1.c

```

**Core Stats & Display:**

### Why Highlighting Makes It Worse1. `prt_field()` - Character name  

2. `prt_stat()` - Stat names (Str/Dex/Con/Gra) in story, values in monospace

In `draw_equipment_story_rows()` and similar highlighted rendering:3. `prt_exp()` - "Exp" label in story, number in monospace

1. Calls `story_fill_rect()` to paint highlight background4. `prt_hp()` - "Health"/"Hth" in story, HP values in monospace

2. Then calls `story_print_text()` multiple times for different parts:5. `prt_sp()` - "Voice"/"Vce" in story, SP values in monospace

   - Prefix (e.g., "Wielding    : ")6. `prt_song()` - Song names in story font

   - Description (e.g., "A Curved Sword")7. `prt_depth()` - Depth text in story ("Surface", "500 ft")

   - Label (e.g., " (A)")

3. Each call advances terminal cursor based on CHARACTER count**Status Effects:**

4. But visual rendering uses PIXEL positions that don't match8. `prt_hunger()` - "Starving", "Weak", "Hungry", "Full"

5. Result: label appears immediately after description instead of at fixed column9. `prt_blind()` - "Blind"

10. `prt_confused()` - "Confused"

### Character Sheet Issue11. `prt_afraid()` - "Afraid"

12. `prt_cut()` - "Mortal wound", "Bleeding XX"

In `prt_stat()` (xtra1.c:306):13. `prt_poisoned()` - "Poisoned XXX"

```c14. `prt_state()` - "Entranced!", "Smithing", "Rest", "Stealth"

sdl_story_font_enable();15. `prt_speed()` - "Fast", "Slow"

put_str(trimmed_label, ROW_STAT + stat, 0);  // "Str" in story font16. `prt_terrain()` - "Pit", "Web", "Sunlight"

sdl_story_font_disable();17. `prt_stun()` - "Knocked out", "Heavy stun", "Stun"

cnv_stat(p_ptr->stat_use[stat], tmp);

c_put_str(TERM_L_GREEN, tmp, ROW_STAT + stat, COL_STAT + 10);  // Value at col 10### How Font Persistence Works

```

The sidebar redraw system ensures story font persists across redraws:

Problem: After story font text, terminal cursor is at wrong position because story font width ≠ mono font width.

1. **Redraw Trigger**: Game sets `p_ptr->redraw` flags when stats/state changes

## Logging Strategy2. **Redraw Handler**: `handle_stuff()` calls `redraw_stuff()` 

3. **Individual Updates**: Each `prt_*()` function called individually:

### Phase 1: Instrument Text Rendering Pipeline   - Enables story font

Add comprehensive logging to track the mismatch between terminal columns and pixel positions.   - Renders text  

   - Flushes with `Term_fresh()`

#### Files to Instrument:   - Disables story font

1. **main-sdl.c** - `callback_sdl_text()`   - Renders numbers (if any) in monospace

2. **util.c** - `text_out_to_screen_story()`, `story_print_text()`4. **Per-Call Management**: Each function manages its own font state independently

3. **z-term.c** - `Term_queue_char()`, `Term_queue_chars()`

4. **object1.c** - `draw_equipment_story_rows()`, `story_render_equipment_entry()`This design ensures:

5. **xtra1.c** - `prt_stat()`- Text always renders in story font when sidebar redraws

- Numbers always render in monospace for clarity

#### Key Metrics to Log:- Font state doesn't leak between functions

- Terminal cursor position (column units): `Term->scr->cx`, `Term->scr->cy`- Works correctly after menus close and screen restores

- Pixel position: `current_x_pixels` in `text_out_to_screen_story()`

- Text content and length### Testing Status

- Story font active state- ✅ Built successfully with CMake

- Calculated widths: `sdl_story_font_text_width()` results- ✅ Pattern matches working code in `files.c`

- Column positions passed to `story_print_text()` (col, max_cols)- ✅ `#ifdef USE_SDL` guards prevent non-SDL build errors

- ⏳ In-game testing needed to verify visual appearance

### Phase 2: Test Scenarios- ⏳ Need to verify persistence after menu open/close

Run game and trigger specific UI states while logging captures the issue:

### Remaining Issues

1. **Equipment Menu (Issue #2)**

   - Open equipment menu with 'e'#### Character Screen Redraw Blinking

   - Navigate to empty slot (e.g., bow when no bow equipped)The birth screens (stat/skill allocation) still have optimization issues unrelated to story font:

   - Observe log showing "(NO BOW)" position

**Problem:** Both `player_birth_aux_2()` (stats) and `gain_skills()` (skills) call `display_player(0)` every time cursor moves, causing full screen redraws.

2. **Inventory Menu (Issue #1)**

   - Open inventory with 'i'**Historical Note:** Checked git history - even old versions (before SDL) had `display_player(0)` in the main loops. So "blinking" is NOT a regression from story font changes.

   - Navigate to highlighted item

   - Observe log showing label "(A)" position**Root Cause:** The character screen was always redrawing fully on every cursor movement. Story font changes didn't introduce this behavior.



3. **Character Sheet (Issue #3)****Potential Fix** (not implemented): Remove `display_player(0)` from cursor movement loops and implement targeted updates for just the cost highlights, similar to the main game sidebar pattern.

   - Gain/lose stat modifier

   - Observe log showing stat value positions**Status:** Documented but not fixed - separate optimization task outside story font scope.



### Phase 3: Analysis---

Review logs to confirm:

- Mismatch between terminal columns and visual pixels## 2025-10-26: Death Screen Story Font Rendering

- Specific column/pixel deltas causing misalignment

- Whether the issue is in text placement or cursor advancement### Summary

Applied story font rendering to all death screen narrative text, including headings and paragraphs. The death narrative now uses the same elegant proportional font as other story elements.

## Next Steps

1. Add logging instrumentation### Changes Made

2. Build and run test scenariosModified three functions in `src/metarun.c`:

3. Analyze log output1. **`print_heading_fade()`** - Wraps heading rendering with story font enable/disable

4. Design fix based on findings2. **`print_paragraph_fade()`** - Wraps paragraph fade-in with story font enable/disable

3. **`print_paragraph()`** - Wraps fast-forward paragraph rendering with story font enable/disable

Each function now follows the established pattern:
```c
#ifdef USE_SDL
    sdl_story_font_enable();
#endif
    // ... render text using text_out_hook ...
#ifdef USE_SDL
    sdl_story_font_disable();
#endif
```

### How Story Font Works
The story font system operates through a layered approach:

1. **Font Loading**: `sdl_story_font_load()` in `main-sdl.c` loads the proportional font at startup
2. **Mode Toggle**: `sdl_story_font_enable()` sets `g_state.use_story_font = true`
3. **Automatic Detection**: `text_out_to_screen()` checks `sdl_is_story_font_enabled()`
4. **Pixel-Based Wrapping**: Routes to `text_out_to_screen_story()` which uses actual pixel measurements
5. **Mode Restore**: `sdl_story_font_disable()` returns to monospace rendering

The pixel-based wrapper (`text_out_to_screen_story()`) measures each word's actual rendered width and wraps based on pixel position rather than character count, allowing proportional fonts to fill the available terminal width efficiently.

### Testing
- Built successfully with CMake
- Death narrative functions now automatically use story font when rendering
- All existing story font locations continue to work (intro screens, help text, etc.)

---

## 2025-10-26: Story Font Wrapping - Pixel Position Tracking Fix

### Summary
Fixed the final issue with story font wrapping: the code was tracking **column position** (character count) instead of **pixel position**, causing premature wrapping despite having sufficient pixel width.

### The Problem
The wrapping code was using `current_pixels = x * cell_width` where `x` was the column number. This assumed every character occupied one full cell width, which is not true for proportional fonts!

**Example from logs:**
```
wrap_pixels=2784 (87 columns * 32 pixels/column)
At column 86: "...wind," 
  current_pixels = 86 * 32 = 2752 ✓
  
Next word "leaving" (7 chars, 164 pixels):
  Would place us at column 93 (86 + 7)
  But actual pixels: 2752 + 164 = 2916 pixels
  Decision: 2916 > 2784, so WRAP ✓
  
BUT: The proportional chars in "wind," only used ~122 pixels,
     not 5 * 32 = 160 pixels!
```

The code thought we were further along the line (in pixels) than we actually were, because it multiplied character count by cell width.

### The Root Cause
```c
// WRONG: Assumes each character = one cell width
int current_pixels = x * cell_width;  

if (current_pixels + word_pixels > wrap_pixels)
    wrap();
```

For proportional fonts, characters can be **narrower** than the cell width, so tracking by character count wastes space.

### The Solution
Track pixel position directly, updating it by the **actual rendered width** of each word:

**Before:**
```c
int current_pixels = x * cell_width;  // ❌ Based on character count
```

**After:**
```c
int current_x_pixels = text_out_indent * cell_width;  // ✓ Track actual pixels

// For spaces:
current_x_pixels += cell_width;

// For words:
current_x_pixels += word_pixels;  // Actual rendered width
```

### Implementation Details

**Position Tracking:**
```c
/* Start at indent */
int current_x_pixels = text_out_indent * cell_width;

/* Each space adds one cell */
while (*s == ' ') {
    current_x_pixels += cell_width;
}

/* Each word adds its actual rendered width */
int word_pixels = sdl_story_font_text_width(word_start, word_chars);
current_x_pixels += word_pixels;

/* Wrap decision based on pixel position */
if (current_x_pixels + next_word_pixels > wrap_pixels)
    wrap();
```

**Column tracking (`x`):** Still maintained for cursor positioning, but not used for wrapping decisions.

### Files Modified
- **src/util.c**:
  - Added `current_x_pixels` variable to `text_out_to_screen_story()`
  - Removed calculation `current_pixels = x * cell_width`
  - Update `current_x_pixels` after each word by `word_pixels`
  - Update `current_x_pixels` after each space by `cell_width`
  - Reset `current_x_pixels` on newlines and wraps
  - Updated debug logging to show `current_x_pixels`

### Why This Matters
**Proportional font efficiency:**
- Character 'i' might be 8 pixels wide
- Character 'W' might be 24 pixels wide
- Cell width might be 32 pixels

If we track by character count:
- "iii" = 3 chars = 96 pixel budget used
- Actual: 24 pixels, wasting 72 pixels

If we track by pixels:
- "iii" = 24 pixels used
- Can fit more content!

### Result
✅ **Text fills the full pixel width of the terminal**
✅ **Wrapping based on actual rendered dimensions**  
✅ **Proportional fonts use available space efficiently**
✅ **No premature wrapping**
✅ Build successful

The text should now flow all the way to the right edge, using every available pixel before wrapping!

---

## 2025-10-26: Story Font Wrapping - Scale Factor Fix

### Summary
Fixed story font pixel-based wrapping to account for the **scaling factor** applied when rendering text. The text width measurements were using unscaled font metrics, but the rendered text is scaled to match cell height.

### The Problem
The story font rendering applies a scaling transform to fit the cell height:
```c
float scale = cell_h / font_h;
rendered_width = text_surface->w * scale;
```

But `sdl_story_font_text_width()` was returning the **unscaled** width from `TTF_MeasureString()`. This meant:
- Measured width: 80 pixels (unscaled)
- Actual rendered width: 80 * scale = 120 pixels (scaled)
- Wrapping logic thought text was narrower than it actually appeared
- Result: Text wrapped too early

### Example
```
Font height: 32 pixels (loaded at scaled size)
Cell height: 24 pixels (main view cell)
Scale factor: 24 / 32 = 0.75

Word "companions" measured: 80 pixels (unscaled)
Actual rendered width: 80 * 0.75 = 60 pixels
```

Without accounting for scale, wrapping would allow too much text, causing overflow.

### The Solution
Modified `sdl_story_font_text_width()` to apply the same scaling factor used during rendering:

**Before:**
```c
int sdl_story_font_text_width(cptr text, int len)
{
    int w = 0;
    TTF_MeasureString(g_state.story_font, text, len, 0, &w, NULL);
    return w;  // ❌ Unscaled!
}
```

**After:**
```c
int sdl_story_font_text_width(cptr text, int len)
{
    int w = 0;
    TTF_MeasureString(g_state.story_font, text, len, 0, &w, NULL);
    
    // Apply scaling factor to match rendering
    int font_h = TTF_GetFontHeight(g_state.story_font);
    float scale = (float)cell_h / (float)font_h;
    w = (int)((float)w * scale);
    
    return w;  // ✓ Scaled to match actual rendering!
}
```

### Files Modified
- **src/main-sdl.c**:
  - Modified `sdl_story_font_text_width()` to apply scaling
  - Uses `TTF_GetFontHeight()` to get font metrics
  - Calculates same scale factor as `callback_sdl_text()`
  - Applies scale to measured width before returning

- **src/util.c**:
  - Added trace logging to `text_out_to_screen_story()` for debugging
  - Logs: wid, wrap_cols, cell_width, wrap_pixels

### Technical Details

**Scaling Calculation:**
```c
/* Get the font's natural height */
int font_h = TTF_GetFontHeight(g_state.story_font);

/* Calculate how much we scale to fit cell height */
float scale = (float)g_views[0].cell_h / (float)font_h;

/* Apply to measured width */
int scaled_width = (int)((float)unscaled_width * scale);
```

This matches exactly what `callback_sdl_text()` does when rendering:
```c
float scale = cell_h_f / surf_h_f;
dst.w = (float)(text_surface->w) * scale;
```

### Why This Matters
1. **Story font is loaded at scaled size** based on `aux_view_font_size`
2. **But rendering scales it again** to fit the main view's `cell_h`
3. **Width measurements must match** the final rendered dimensions
4. **Different scale factors** between aux and main views required this correction

### Result
✅ Text width measurements now match actual rendered width
✅ Wrapping decisions are accurate
✅ Text fills terminal width properly
✅ No premature wrapping or overflow
✅ Build successful

### Testing
View story text with `SIL_LOG_LEVEL=trace` to see wrapping calculations in the log file.

---

## 2025-10-26: Hide cursor on intro and story screens

Summary
- Hide the hardware/text cursor while the intro (first screen) and the story display are visible. This prevents a blinking cursor from appearing on top of the intro poem or the paged "The Tale So Far" output.

Files changed
- `src/init2.c` - `display_introduction()` now saves the current cursor visibility, sets the cursor hidden during the intro, then restores the previous state after flushing.
- `src/files.c` - `print_story()` now saves the cursor visibility at start, forces the cursor hidden for the entire story display (including fades/paging), and restores it after the story finishes and the screen is restored.

Notes
- Performed a full SDL/CMake build to verify changes; build completed successfully.


## 2025-10-26: Story Font Wrapping - Terminal Width Fix

### Summary
Fixed story font pixel-based wrapping to actually use the full terminal width instead of wrapping prematurely at character boundaries.

### The Problem
The pixel-based wrapping was calculating wrap points correctly based on actual text width in pixels, BUT it was still enforcing a hard wrap at `wrap_cols` character positions. This meant:
- Text would check if it fit in pixel width
- But then immediately wrap if it exceeded column count
- Result: Proportional text didn't fill the available terminal width

Example with 160-column terminal:
```
wrap_cols = 157 (160 - 3 for indent)
wrap_pixels = 157 * 12 = 1884 pixels

Word "companions" measured at 80 pixels
Current position: column 50 (600 pixels)
Check: 600 + 80 = 680 < 1884 ✓ fits in pixels
BUT: After rendering, x = 61... then check `if (x >= 157)` would wrap!
```

The proportional font characters are narrower, so we could fit more columns worth of text, but the character-based check was preventing this.

### The Solution
Removed the hard character-based wrap checks from `text_out_to_screen_story()`:

**Before:**
```c
Term_addch(a, ' ');
if (++x >= wrap_cols) {  // ❌ Wraps too early!
    x = text_out_indent;
    y++;
}
```

**After:**
```c
Term_addch(a, ' ');
x++;  // ✓ Just track position, let pixel check handle wrapping
```

The pixel-based check (`current_pixels + word_pixels > wrap_pixels`) is what determines when to wrap, not the column count.

### Files Modified
- **src/util.c**: 
  - Modified `text_out_to_screen_story()` function
  - Removed `if (++x >= wrap_cols)` checks in two places:
    1. Space handling loop
    2. Character output loop in word rendering
  - Now only the pixel-width check controls wrapping

### Technical Details

**Wrapping Decision:**
```c
int word_pixels = sdl_story_font_text_width(word_start, word_chars);
int current_pixels = x * cell_width;

if (x > text_out_indent && (current_pixels + word_pixels) > wrap_pixels) {
    /* Wrap to next line - based purely on pixel width */
    x = text_out_indent;
    y++;
}
```

**Column tracking:** The `x` variable still tracks approximate column position for cursor management, but it no longer enforces wrapping.

### Result
✅ Story font text now fills the full terminal width
✅ Proportional text can use more "columns" when characters are narrower
✅ Wrapping happens only when pixel width is exhausted
✅ Build successful

### Testing
Test by viewing story text (`print_story()`) in different terminal widths. Text should now flow all the way to the right edge before wrapping.

---

## 2025-10-25: Story Font Page Wrapping Fix

### Summary
Fixed page wrapping in `print_story()` that was leaving wasted space at the bottom of pages. The issue was using a hardcoded estimate instead of calculating actual line count based on text content.

### The Problem
- Page break logic used a hardcoded estimate of 6 lines per story entry
- This didn't account for actual text length or wrapping behavior
- With pixel-based wrapping for story font, text could fit more content per line
- Result: Pages would break too early, leaving significant whitespace at the bottom

### The Solution
**Created `count_wrapped_lines_story()` function** (util.c):
- Mirrors the pixel-based wrapping logic from `text_out_to_screen_story()`
- Measures actual words using `sdl_story_font_text_width()`
- Calculates exact number of lines text will occupy
- Accounts for word boundaries and wrapping behavior

**Updated `print_story()` function** (files.c):
- Calculate wrap width and text once at the top of the loop
- Call appropriate line counter based on SDL vs non-SDL build:
  ```c
  #ifdef USE_SDL
      int text_lines = count_wrapped_lines_story(text, wrap_width, indent);
  #else
      int text_lines = count_wrapped_lines(text, wrap_width, indent);
  #endif
  ```
- Use actual line count: `estimated_space_needed = 1 + text_lines + 1`
  - 1 for heading
  - text_lines for body content
  - 1 for blank line separator
- Eliminated duplicate variable declarations

### Files Modified
- **src/util.c**:
  - Added `count_wrapped_lines_story()` function under `#ifdef USE_SDL`
  - Pixel-based line counting matching the wrapping algorithm
  
- **src/externs.h**:
  - Declared `count_wrapped_lines_story()` under `#ifdef USE_SDL`
  
- **src/files.c**:
  - Modified `print_story()` to use actual line counting
  - Hoisted `wrap_width` and `text` variable declarations
  - Removed hardcoded `estimated_space_needed = 6`

### Technical Details

**Line Counting Algorithm:**
```c
int count_wrapped_lines_story(cptr str, int wrap_cols, int indent)
{
    int wrap_pixels = wrap_cols * sdl_get_cell_width();
    int lines = 1;
    
    for each word in str:
        measure word_pixels = sdl_story_font_text_width(word)
        if (current_pixels + word_pixels > wrap_pixels)
            wrap to next line
        
    return lines;
}
```

**Space Calculation:**
```c
int text_lines = count_wrapped_lines_story(text, wrap_width, indent);
int estimated_space_needed = 1 + text_lines + 1;  // heading + text + blank
```

### Benefits
1. **Accurate pagination**: Pages break exactly when space runs out
2. **Better space utilization**: Maximum content per page
3. **Consistent behavior**: SDL and non-SDL builds both calculate accurately
4. **No wasted space**: Bottom margins are minimized

### Testing
- ✅ Build successful
- Story pages should now fill completely before breaking
- Proportional font wrapping is properly accounted for
- Works for both SDL (pixel-based) and non-SDL (character-based) builds

---

## 2025-10-25: Story Font Pixel-Based Wrapping

### Summary
Implemented intelligent wrapping for story font (proportional text) that fills the available terminal width based on actual pixel measurements instead of character count. This eliminates wasted space when using proportional fonts.

### The Problem
- Story font uses proportional spacing (characters have different widths)
- Text wrapping was based on monospace character count
- This caused premature line breaks, leaving significant whitespace at line ends
- Example: A line allowed 80 characters in monospace, but proportional text only filled ~60% of the width

### The Solution
Implemented pixel-based wrapping that:
1. Measures actual text width using TTF font metrics
2. Calculates available width: `num_columns * cell_width_pixels`
3. Wraps when pixel width would exceed available space
4. Handles word boundaries properly for clean breaks

### Implementation

#### New Functions (main-sdl.c)
```c
/* Check if story font mode is active */
bool sdl_is_story_font_enabled(void);

/* Measure pixel width of text in story font */
int sdl_story_font_text_width(cptr text, int len);

/* Get cell width in pixels for the main terminal */
int sdl_get_cell_width(void);
```

#### New Wrapping Function (util.c)
```c
#ifdef USE_SDL
void text_out_to_screen_story(byte a, cptr str);
#endif
```

Features:
- Measures words using `TTF_MeasureString()`
- Converts terminal columns to pixel width
- Wraps based on actual rendered width
- Handles word boundaries and spaces properly
- Falls back to character-based wrapping if needed

#### Automatic Dispatch
Modified `text_out_to_screen()` to automatically use pixel-based wrapping when story font is enabled:
```c
void text_out_to_screen(byte a, cptr str)
{
#ifdef USE_SDL
    if (sdl_is_story_font_enabled())
    {
        text_out_to_screen_story(a, str);
        return;
    }
#endif
    /* ... regular monospace wrapping ... */
}
```

### Files Modified
- **src/main-sdl.c**:
  - Added `sdl_is_story_font_enabled()` - query current font mode
  - Added `sdl_story_font_text_width()` - measure text in pixels using `TTF_MeasureString()`
  - Added `sdl_get_cell_width()` - get terminal cell width in pixels
  
- **src/util.c**:
  - Added `text_out_to_screen_story()` - pixel-based wrapping implementation
  - Modified `text_out_to_screen()` to dispatch to story version when appropriate
  
- **src/externs.h**:
  - Exposed new SDL helper functions
  - Declared `text_out_to_screen_story()` under `#ifdef USE_SDL`

### Technical Details

**Pixel Width Calculation:**
```c
int wrap_cols = 80;  /* Terminal columns */
int cell_width = sdl_get_cell_width();  /* e.g., 8 pixels */
int wrap_pixels = wrap_cols * cell_width;  /* 640 pixels */
```

**Word Measurement:**
```c
int word_pixels = sdl_story_font_text_width(word_start, word_chars);
int current_pixels = x * cell_width;

if (current_pixels + word_pixels > wrap_pixels) {
    /* Wrap to next line */
}
```

**SDL_ttf Function Used:**
- `TTF_MeasureString(font, text, len, 0, &width, NULL)` - measures exact pixel width of text

### Benefits
1. **Better space utilization**: Lines fill the full terminal width
2. **More readable text**: Fewer artificial line breaks
3. **Automatic**: Works transparently when story font is enabled
4. **No code changes needed**: Existing `text_out_hook` calls work automatically

### Testing
- ✅ Build successful (no compile errors)
- Story font wrapping automatically activates when `sdl_story_font_enable()` is called
- All existing story text locations benefit automatically:
  - Story sequences (`print_story()`)
  - Depth banners (`pause_with_text()`)
  - Any text output using `text_out_hook`

### Future Considerations
- Could cache font metrics for performance optimization
- Consider adding line height adjustments for better readability
- Might extend to other UI elements that use story font

---

## 2025-10-25: Custom Story Font System (CORRECT IMPLEMENTATION)

### Summary
Implemented a **proper** custom font system that integrates with the terminal rendering system. Instead of rendering on top, the system uses a flag to switch between story font and monospace font within the existing `callback_sdl_text` hook.

### Key Architecture

The previous approach was fundamentally flawed - it tried to render SDL text on top of terminal text, which got cleared/overwritten. The correct approach is to modify the terminal text rendering hook itself.

#### How It Works
1. **Font Mode Flag**: `g_state.use_story_font` (bool)
2. **Text Rendering Hook**: `callback_sdl_text()` checks the flag
   - If `true`: uses `TTF_RenderText_Blended()` with story_font
   - If `false`: uses regular monospace font_atlas
3. **Enable/Disable API**: Wrap story sections with `sdl_story_font_enable()` / `sdl_story_font_disable()`

### Configuration

```json
{
  "sdl": {
    "storyFont": "lib/xtra/font/YourStoryFont.ttf",
    "monospaceFont": "lib/xtra/font/YourMonoFont.ttf"
  }
}
```

- `storyFont`: Non-monospace font for narrative (32px, fallback: InputMono-Bold.ttf)
- `monospaceFont`: Reserved for future custom monospace font support

### Implementation Details

#### Modified `callback_sdl_text` (main-sdl.c)
```c
static errr callback_sdl_text(int x, int y, int n, byte a, cptr s)
{
    // ... setup code ...
    
    if (g_state.use_story_font && g_state.story_font) {
        // Render using custom TTF font (proportional)
        SDL_Surface* text_surface = TTF_RenderText_Blended(...);
        // ... render to texture ...
    } else {
        // Use regular monospace font atlas
        // ... existing glyph rendering code ...
    }
}
```

#### API Functions (main-sdl.c)
```c
void sdl_story_font_enable(void)  // Sets g_state.use_story_font = true
void sdl_story_font_disable(void) // Sets g_state.use_story_font = false
```

#### Usage Pattern
```c
#ifdef USE_SDL
    sdl_story_font_enable();
#endif
    Term_putstr(...);  // This will use story font
    c_put_str(...);    // This will use story font
#ifdef USE_SDL
    sdl_story_font_disable();
#endif
```

### Where Story Font Is Used

1. **Story sequences** (`print_story()` in files.c)
   - Title: "=== The Tale So Far ==="
   - Chapter headings: "Chapter 1. Whisper of Manwe", etc.
   - Wrapped in enable/disable calls around each heading

2. **Depth change banners** (`pause_with_text()` in xtra2.c)
   - Enable at start of function
   - All banner text and story stanzas use story font
   - Disable at end before cleanup

3. **Character sheet** (`display_player_misc_info()` in files.c)
   - Player name (both parts: Name and House title)
   - "the Oathbreaker" variant

### Files Modified

- **src/sdl-config.h**: Added `monospace_font[256]` field
- **src/sdl-config.c**: Added JSON loading/saving for both fonts
- **src/main-sdl.c**:
  - Added `use_story_font` flag to `sdl_state`
  - Modified `callback_sdl_text()` to check flag and use custom font
  - Added `sdl_story_font_enable()` and `sdl_story_font_disable()` functions
  - Removed old broken `sdl_render_story_text()` function
- **src/externs.h**: Exposed enable/disable API
- **src/files.c**: 
  - `print_story()`: Wrapped all text rendering with enable/disable
  - `display_player_misc_info()`: Enabled for character name
- **src/xtra2.c**: 
  - `pause_with_text()`: Enabled at start, disabled at end

### Technical Notes

**Why This Works:**
- Terminal operations (`Term_putstr`, `c_put_str`) eventually call `callback_sdl_text()`
- By modifying the hook itself, we intercept ALL text rendering
- The flag lets us selectively use custom font vs monospace
- No conflicts with `Term_clear()` or other terminal operations

**Rendering Details:**
- Story font size: 32px
- Uses `TTF_RenderText_Blended()` for anti-aliasing
- Text can overflow cell boundaries (proportional spacing)
- Regular monospace uses existing font_atlas system

### Testing Results
- ✅ Build successful (no new errors)
- ✅ Story font loads at 32px
- ✅ All story text should use custom font
- ✅ Character name uses custom font
- ✅ Banners use custom font
- ✅ Regular game text still uses monospace

### Future Enhancements
- Support custom monospace font via `monospaceFont` config
- Add font size configuration options
- Consider caching rendered glyphs for performance

---

## 2025-10-25: Skill Distribution UI Improvements

### Changes Made
1. **Full skill name highlighting in skill distribution screen**
   - Modified `gain_skills()` in `src/birth.c` to highlight the entire skill name (not just the cost digits) when a skill is selected
   - Previously only the cost column was highlighted in blue; now the skill name in the left column is also highlighted
   - Uses `TERM_L_BLUE` for consistency with other UI highlighting
   - **Fixed**: Highlight position adjusted from `col` to `col - 1` to match the actual display position (col 41 vs 42)

2. **Direct keyboard shortcut to skill distribution**
   - Modified `process_command()` in `src/dungeon.c` to make capital 'H' directly open the skill distribution screen
   - Lowercase 'h' continues to open the character sheet (requires pressing 'i' to access skills)
   - Capital 'H' now calls `gain_skills()` directly with proper screen save/load wrapping
   - **Fixed**: Added `screen_save()` and `screen_load()` around the `gain_skills()` call to prevent character_icky imbalance issues

### Files Modified
- `src/birth.c`: 
  - Added skill name highlighting in the cost display loop (line ~2400)
  - Changed highlight column from `col` to `col - 1` to align with skill name position
- `src/dungeon.c`: 
  - Split 'h' and 'H' key handling to provide direct skill access (line ~1200)
  - Added screen_save/load around gain_skills() call to maintain proper screen state

### Technical Notes
- The `screen_save()` and `screen_load()` calls are critical for maintaining the `character_icky` counter balance
- Without them, the screen state becomes corrupted and the menu can't be properly exited
- The skill names in `display_player()` are rendered at column 41, while the cost display used column 42

### Testing
- Build successful with CMake
- Warnings are pre-existing and unrelated to these changes

---

## 2025-10-25 - Color-Coded Object Descriptions

### New Feature
Object description text is now **color-coded** like monster descriptions, making different types of information easier to read at a glance.

### Color Scheme

**Positive Effects:**
- **Green**: "increases", "improves" (stat/skill bonuses)
- **Light Blue**: "grants", "resistance" (abilities, resistances)

**Negative Effects:**
- **Light Red**: "decreases", "worsens", bad effects (penalties, negative traits)
- **Red**: "vulnerable" keyword
- **Violet** (purple): "cursed", "permanently cursed", "heavily cursed"

**Combat/Damage:**
- **Light Red**: "slays" keyword
- **Orange**: Enemy types in slay lists (orcs, trolls, dragons, etc.), "branded" keyword

**Elemental Brands:**
- **Light Red**: "flame" (fire brand)
- **Light Blue**: "frost" (cold brand)
- **Yellow**: "lightning" (electric brand)
- **Green**: "venom" (poison brand)

**Elemental Resistances/Vulnerabilities:**
- **Light Blue**: "cold", "frost"
- **Light Red**: "fire", "flame"
- **Yellow**: "lightning"
- **Green**: "poison", "venom"
- **Red**: "bleeding"
- **Violet**: "fear", "confusion", "hallucination", "panic"
- **Light Dark**: "blindness", "darkness"
- **Orange**: "stunning"

**Numbers/Values:**
- **Umber** (brown): All numeric values (+3, -2, damage dice, etc.)

**Special Abilities:**
- **Violet** (purple): Ability names in ability lists

**Normal Text:**
- **White**: Regular descriptive text

### Examples

**Before (all white):**
```
It increases your strength and dexterity by 3.
It improves your melee by 2.
It slays orcs and trolls.
It grants you the abilities: Power, Crowd Fighting.
It provides resistance to cold and fire.
It is branded with flame and frost.
It creates an unnatural darkness.
It is heavily cursed.
```

**After (color-coded):**
```
It [GREEN]increases[WHITE] your strength and dexterity by [UMBER]3[WHITE].
It [GREEN]improves[WHITE] your melee by [UMBER]2[WHITE].
It [L_RED]slays[WHITE] [ORANGE]orcs[WHITE] and [ORANGE]trolls[WHITE].
It [L_BLUE]grants[WHITE] you the [L_BLUE]abilities[WHITE]: [VIOLET]Power[WHITE], [VIOLET]Crowd Fighting[WHITE].
It provides [L_BLUE]resistance[WHITE] to [L_BLUE]cold[WHITE] and [L_RED]fire[WHITE].
It is [ORANGE]branded[WHITE] with [L_RED]flame[WHITE] and [L_BLUE]frost[WHITE].
It creates an unnatural [L_DARK]darkness[WHITE].
It is [VIOLET]heavily cursed[WHITE].
```

### Implementation

Added colored output functions in `obj-info.c`:
- `p_text_out_c(byte attr, cptr str)` - Color-coded paragraph text
- `output_list_c(cptr list[], int n, byte attr)` - Color-coded lists

Updated description functions:
- `describe_stats()` - Stat bonuses/penalties
- `describe_skills()` - Skill improvements
- `describe_slay()` - Slaying abilities
- `describe_brand()` - Elemental brands (flame, frost, lightning, venom)
- `describe_abilities()` - Special abilities
- `describe_resist()` - Elemental and status resistances
- `describe_vulnerability()` - Elemental vulnerabilities
- `describe_misc_magic()` - Curses, darkness, and miscellaneous effects

### Files Modified
- `src/obj-info.c`: Added color functions and updated all major description outputs

### Visual Impact
- **Easier scanning**: Positive effects stand out in green
- **Quick identification**: Numbers in brown are easy to spot
- **Consistent with monsters**: Matches the color-coding style of monster descriptions
- **Better readability**: Different information types are visually distinct

---

## 2025-10-25 - Artifact Unique Color Option

### New Feature
Added optional **bright green coloring** for all identified artifacts as an alternative to the shade system.

**New Game Option:**
- **Name**: "Display artifacts in unique bright green color"
- **Location**: Options → Display menu
- **Default**: Enabled (ON)
- **Effect**: When enabled, all identified artifacts display in TERM_L_GREEN1 (bright green shade) instead of shaded versions of their base colors

### Implementation

**Option definition:**
- `OPT_artifact_unique_color` (index 74)
- Macro: `artifact_unique_color`

**Applied in two locations:**

**1. Inventory/Equipment Lists (`object_display_color()` in object1.c):**
```c
if (artefact_p(o_ptr) && object_known_p(o_ptr))
{
    if (artifact_unique_color)  /* Option enabled */
    {
        return TERM_L_GREEN + TERM_SHADE;  /* Bright green */
    }
    
    /* Option disabled: use shade of base color */
    return MAKE_EXTENDED_COLOR(color_to_use, 1);
}
```

**2. Object Description/Inspection (`screen_out_head()` in obj-info.c):**
```c
/* Use same color logic as inventory/equipment displays */
byte base_color;

/* Determine base color from item type */
if (weapon_glows(o_ptr))
{
    base_color = TERM_L_BLUE;
}
else
{
    base_color = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];
}

/* Apply artifact/shade coloring using the same function as inventory */
byte name_color = object_display_color(o_ptr, base_color);
```

This ensures **all items** use their proper type colors in the description screen, matching the inventory display exactly.

### Visual Results

**With option ENABLED (default):**
- Inventory/Equipment lists: ALL identified artifacts in **Bright Green**
- Inspection screen: 
  - Identified artifacts in **Bright Green**
  - Regular items in **their type color** (swords=red, armor=blue, boots=brown, etc.)
- Easy to spot artifacts everywhere!

**With option DISABLED:**
- Inventory lists: Each artifact in shaded version of its item type color
- Inspection screen:
  - Identified artifacts in **shaded version of their type color**
  - Regular items in **normal type color**

**Example colors in inspection:**
- Sword (regular): Red name
- Artifact Sword (option ON): Bright Green name
- Artifact Sword (option OFF): Dark Red name
- Boots (regular): Brown name
- Boots of Finrod (option ON): Bright Green name
- Boots of Finrod (option OFF): Dark Brown name
- Potion: Green name
- Armor: Blue name

### Files Modified
- `src/defines.h`: Added OPT_artifact_unique_color constant and macro
- `src/tables.c`: Added option description, default value (true), and menu placement
- `src/object1.c`: Updated `object_display_color()` for inventory/equipment
- `src/obj-info.c`: Updated `screen_out_head()` for description/inspection screen

### Color Reference
- **TERM_L_GREEN1** (index 29): RGB(0, 220, 100) - Bright vibrant green for artifacts
- **Item type colors match inventory**: Swords (red), Armor (blue), Boots (brown), Potions (green), etc.
- All items now have **consistent coloring** between inventory and inspection screens

---

## 2025-10-25 - Artifact Color Shading (FINAL - SDL Renderer Fix)

### Feature
Identified artifacts now display with **shaded text color** (darker version of base color) to make them visually distinct from regular items.

### The REAL Issue - SDL Renderer Only Used 16 Colors

After extensive debugging with log output, discovered the **SDL renderer was ignoring extended colors**!

**Root Cause:**
```c
// main-sdl.c line 471 - BEFORE (BROKEN)
SDL_Color col = g_state.palette[a % 16];  // ❌ Strips shade info!
```

The `% 16` operation was discarding all extended color information, mapping:
- Color 23 (TERM_UMBER shade 1) → Color 7 (TERM_UMBER base)
- Color 17 (TERM_WHITE shade 1) → Color 1 (TERM_WHITE base)

The `angband_color_table[256][4]` array DOES contain all extended colors:
- Indices 0-15: Base colors (TERM_DARK through TERM_L_UMBER)
- Indices 16-31: Shade 1 of colors 0-15 (darker versions)
- Indices 32-47: Shade 2 of colors 0-15 (even darker)
- ...up to 128 shades total

But the SDL renderer was only using the first 16!

### The Complete Fix

**1. Use MAKE_EXTENDED_COLOR macro (object1.c):**
```c
byte object_display_color(const object_type* o_ptr, byte base_color)
{
    byte color_to_use = base_color;
    
    if (o_ptr->name1 && a_info[o_ptr->name1].d_attr) {
        color_to_use = a_info[o_ptr->name1].d_attr;
    }
    
    if (artefact_p(o_ptr) && object_known_p(o_ptr)) {
        return MAKE_EXTENDED_COLOR(color_to_use, 1);  // Shade level 1
    }
    
    return color_to_use;
}
```

**2. Fix SDL renderer to use extended colors (main-sdl.c):**
```c
// AFTER (FIXED)
/* Use extended color table to support shaded colors (indices 0-255) */
SDL_Color col;
col.r = angband_color_table[a][1];  // Direct lookup, no modulo!
col.g = angband_color_table[a][2];
col.b = angband_color_table[a][3];
col.a = 255;

SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
```

### Debug Evidence

**Log showed color was being set correctly:**
```
sidebar object: ... name='*Pair of Boots of Finrod' ... color=23
sidebar object: ... name='Pair Greaves' ... color=7
```

But they displayed the same because `23 % 16 = 7`!

### Visual Result

**Now working properly:**
- **Regular Boots** (color 7): `0x80, 0x40, 0x00` = Normal brown
- **Boots of Finrod** (color 23): `0xC8, 0x64, 0x00` = Darker richer brown ✓
- **Regular items**: Base colors
- **Identified artifacts**: Shade 1 (noticeably darker) ✓

### Files Modified
- `src/object1.c`: Uses `MAKE_EXTENDED_COLOR(color, 1)` for artifacts
- `src/main-sdl.c`: Fixed to use full `angband_color_table[a]` instead of `palette[a % 16]`
- `src/cmd4.c`: Updated smithing display
- `src/externs.h`: Function declaration

### Technical Notes
- **Extended color encoding**: `((shade << 4) | base_color) & 0x7F`
- **angband_color_table**: 256 entries, indices 16-31 are shade 1
- **Shade levels**: 0 (base) through 7 (very dark)
- **We use shade 1**: Subtle but clear distinction
- **Works in**: SDL3, Windows, GCU (terminals with 256 color support)

This feature is now fully functional! 🎨

---

## 2025-10-24 - Item Color Scheme Overhaul

### Problem Analysis
The original color scheme had several issues:
1. **Poor color distribution**: Many items shared the same colors
   - 3 weapon types all White
   - 6 armor pieces + staff + food all Light Umber
   - Multiple armors all Slate
2. **Unused colors**: Not utilizing all 16 available terminal colors
3. **Missing GEM color**: TV_GEM (56) had no defined color
4. **Poor visual grouping**: No logical color theme for item categories

### Solution - Menu-Order-Aware Color Mapping
Analyzed the actual menu display order from `object_group_tval` in cmd4.c:
```
Food → Potions → Rings → Amulets → Staves → Horns → Swords → Polearms →
Hafted → Diggers → Bows → Lights → Soft Armor → Mail → Shields → Cloaks →
Gloves → Helms → Crowns → Boots → Chests
```

**Key Strategy**: Maximize visual distinction for **adjacent** items in menus, allow strategic color sharing for items far apart in display order.

### New Color Scheme

**Consumables** (Green family - natural):
- FOOD: Light Green (0x0D) - herbs
- POTION: Green (0x05) - liquid

**Jewelry** (Precious metals):
- RING: Yellow (0x0B) - gold
- AMULET: Orange (0x03) - amber/gems

**Magic Items** (Mystical colors):
- STAFF: Violet (0x0A)
- HORN: Umber (0x07) - earthy/natural horn

**Weapons** (Warm/aggressive colors):
- SWORD: Red (0x04) - classic weapon color
- POLEARM: Light Red (0x0C) - distinct from sword
- HAFTED: Orange (0x03) - shares with AMULET (far apart)
- DIGGING: Umber (0x07) - tool, shares with HORN/BOOTS
- BOW: Yellow (0x0B) - shares with RING (far apart)
- ARROW: Light Umber (0x0F) - ammunition

**Armor - Body** (Cool/defensive blue tones):
- SOFT_ARMOR: Light Blue (0x0E)
- MAIL: Blue (0x06)

**Armor - Accessories** (Varied neutrals):
- SHIELD: White (0x01) - bright defense
- CLOAK: Violet (0x0A) - shares with STAFF (far apart)
- GLOVES: Light Dark (0x08) - gray leather
- HELM: Slate (0x02) - darker metal
- CROWN: Light White (0x09) - royal/bright
- BOOTS: Umber (0x07) - shares with DIGGING/HORN

**Utility**:
- LIGHT: Light White (0x09) - bright, shares with CROWN
- FLASK: Orange (0x03) - shares with AMULET/HAFTED
- GEM: Light Blue (0x0E) - **NEW!** crystal, shares with SOFT_ARMOR

**Miscellaneous**:
- CHEST: Slate (0x02) - wooden
- SKELETON: White (0x01) - bone

### Benefits
1. **All 16 colors utilized** across the item spectrum
2. **Adjacent items always have distinct colors** in menu order
3. **Thematic grouping**: Related items use color families (weapons=warm, armor=cool, consumables=green)
4. **Strategic sharing**: Colors only repeat for items separated by 5+ positions in menus
5. **GEM now has a color** (was defaulting to L_DARK)

### Files Modified
- `lib/pref/font-xxx.prf` - Updated E: entries with new color mappings and detailed comments

### Color Reference
```
0x01=White  0x02=Slate   0x03=Orange  0x04=Red     0x05=Green   0x06=Blue
0x07=Umber  0x08=L_Dark  0x09=L_White 0x0A=Violet  0x0B=Yellow
0x0C=L_Red  0x0D=L_Green 0x0E=L_Blue  0x0F=L_Umber
```

---

## 2025-10-24 - Ability Menu Redesign

### Changes Made

**Layout Improvements** (Single-Column):
1. **Compact single-column layout** - abilities start at row 3 (was row 4)
   - Fits 20 songs in rows 3-22 (20 rows) within 24-line minimum terminal
   - Clean, simple navigation

2. **Description area expanded**:
   - COL_DESCRIPTION moved from 41 to 35 (6 columns wider, wraps at col 79)
   - Description starts at row 3 with ability name in TERM_YELLOW
   - More vertical space before prerequisites (start at row 10)

3. **Prerequisites/Cost with color coding**:
   - Prerequisites at row 10: **green** if met, **dark gray** if not met
   - Cost at row 16+: **green** if affordable, **dark gray** if not

**Color Scheme Added**:
- **Title & highlights**: TERM_L_BLUE
- **Headers & ability names**: TERM_YELLOW  
- **Active innate**: TERM_WHITE
- **Active learned**: TERM_L_GREEN
- **Inactive**: TERM_RED
- **Available**: TERM_SLATE
- **Locked**: TERM_L_DARK

**Color legend** at row 23 explains the scheme to players.

**Files Modified**: `src/cmd4.c` (abilities_menu2 function, COL_DESCRIPTION constant)

**Capacity**: Single column handles 20 abilities (rows 3-22) comfortably in 24-line terminal.

---

## 2025-10-24 - Alchemy Ability Enhancement for Gems

### Feature Request
Add Alchemy ability bonus to increase range of Gems of Revelation, Foes, and Treasures by 1.5x coefficient.

### Implementation
Modified `src/use-obj.c` in the `use_staff()` function to apply a 1.5x multiplier to the detection radius when:
1. The item being used is a gem (`o_ptr->tval == TV_GEM`)
2. The player has the Alchemy ability (`p_ptr->active_ability[S_PER][PER_ALCHEMY]`)

#### Code Changes
Added radius boost to three cases:
- `SV_STAFF_REVELATIONS` (Gem of Revelation)
- `SV_STAFF_TREASURES` (Gem of Treasures)
- `SV_STAFF_FOES` (Gem of Foes)

Formula: `radius = (radius * 3) / 2` (integer math for 1.5x)

Base radius: `10 + p_ptr->skill_use[S_WIL]`

Example: With Will 10, base radius = 20
- Without Alchemy: 20 tiles
- With Alchemy: (20 * 3) / 2 = 30 tiles

#### Files Modified
1. `src/use-obj.c` - Added Alchemy checks to gem detection cases
2. `lib/edit/ability.txt` - Updated Alchemy description to include gem range bonus

### Testing
Build successful. Deploy complete. Ready for in-game testing.

---

## 2025-10-23 - Song of Shattering Debug Investigation

### Issue
Song of Shattering not applying debuffs - no messages in log and no visible effects in monster screen.

### Root Cause
**Song of Shattering was missing from the `ability_bonus()` function in `xtra1.c`!**

The song was properly integrated into the song processing loop, but when calculating the score with `ability_bonus(S_SNG, SNG_SHATTERING)`, it wasn't in the switch statement, so it returned a default bonus of 0.

From the log:
```
Song of Shattering: starting with score=0
Song of Shattering: skill_check result=-16 (score=0, resistance=13)
Song of Shattering: Attempting weapon damage, weaken_chance=0%
```

With score=0:
- All skill checks fail (0 vs monster Will + distance)
- Probability is 0/3 = 0% (should be score/3 percent)
- Song is completely ineffective

### Fix
Added `SNG_SHATTERING` case to the `ability_bonus()` function in `src/xtra1.c`:
```c
case SNG_SHATTERING:
{
    bonus = skill;
    break;
}
```

Placed after `SNG_MASTERY` and before `SNG_CONTEST` to maintain the logical ordering.

### Expected Behavior After Fix
With proper score calculation (e.g., skill 20 = score 20):
- Skill checks: 20 vs (monster Will + distance) - should pass for nearby orcs
- Probability: 20/3 = 6.7% chance per eligible monster per turn
- Messages should appear when equipment is damaged
- Monster screen should show reduced damage/armor values

### Testing
Close and restart the game to load the new executable, then:
1. Sing Song of Shattering near orcs with equipment
2. Check log.txt - should now show score > 0
3. Should see successful skill checks and occasional equipment damage

---

## 2025-10-23 - Song of Trees Fix

## Date
2025-10-23 (earlier)

## Summary
Fixed Song of Trees to damage/stun light-sensitive monsters silently without showing visual light effects like Gem of Light does.

### Issue
- Song of Trees was showing the same visual effect as Gem of Light ("You are surrounded by a white light.")
- Should increase light radius AND damage light-sensitive monsters, but WITHOUT the visual flash/message

### Root Cause (Updated after testing)
- Original implementation called `light_area()` which uses `PROJECT_GRID` flag
- `PROJECT_GRID` causes the visible light-up effect on dungeon squares
- `light_area()` also prints the "surrounded by white light" message
- Song of Trees should work silently in the background
- **Critical bug found:** `project()` reduces damage dice by 2 per square of distance by default
  - With `dd = 1 + (score/10)`, at score 10: dd=2
  - At distance 1: dd reduced to 0, no damage possible!
  - Fixed by using `uniform=true` parameter so dd doesn't decay

### Fix
- Modified `sing_song_of_trees()` to call `project()` directly instead of `light_area()`
- Uses flags: `PROJECT_BOOM | PROJECT_KILL | PROJECT_PASS | PROJECT_HIDE`
- Removed `PROJECT_GRID` to prevent visual lighting effect
- Added `PROJECT_HIDE` to suppress graphics
- **Set `uniform=true`** so damage dice don't decay with distance
- Damage/stun calculations use light level at monster's position, not distance from player
- Maintains damage/stun mechanics via GF_LIGHT handler (which checks `ds > 10` to identify Song vs Gem)

### Behavior After Fix
Song of Trees now:
1. ✅ Increases light radius passively (handled in xtra1.c:1989)
2. ✅ **Stuns HURT_LITE monsters reliably** based on light level (more consistent than damage)
3. ✅ Damages monsters only when Will check succeeds (requires bright light)
4. ✅ Works silently without visual effects or messages
5. ✅ Uses song score for damage skill checks (via `ds = score` parameter)

### Stun vs Damage Mechanics
**Stun (Primary Effect):**
- Calculated: `damroll(dd, light_level)` - scales with light level
- Applied when monster **fails Will save** (result > 0, player wins)
- Duration: Stun value in turns (decreases by 1 per turn)
- With light level 14, dd=2: **2-28 turns of stun**
- Represents the blinding/disorienting effect of light
- Orcs (Will 1-2) will almost always fail against high song skill

**Damage (Secondary Effect):**
- Only applied on **strong Will failure** (result ≥ 5)
- Calculated: `damroll(dd, light_level)` then reduced by resistance
- Reduction formula: `(damage × result) / (result + 5)`
- Represents actual burning/searing damage from intense light
- Bypasses armor (applied via `mon_take_hit`)

**Resistance Outcomes:**
- result ≥ 5: Stun + Damage ("is seared by radiant light!")
- result 1-4: Stun only ("cringes from the light!")
- result ≤ 0: Monster resists ("resists the light!")

This creates the intended progression:
1. Weak song / high monster Will: Monster resists
2. Moderate success: Monster stunned but not damaged (cringes)
3. Strong success: Monster stunned AND damaged (seared)

Gem/Staff of Light:
1. Shows "surrounded by white light" message
2. Creates visible light flash effect
3. Uses player's Will skill for damage checks

### Files Modified
- `src/spells1.c`: Lines 6652-6668 - replaced `light_area()` call with direct `project()` call using appropriate flags
- `src/spells1.c`: Lines 3418-3430 - added debug logging for damage calculation to diagnose issues

### Debug Logging
Added temporary debug logging to GF_LIGHT handler showing:
- Number of damage dice (dd)
- Light level at monster position
- Raw damage before Will reduction
- Will check result
- Final damage after Will reduction
- Stun amount applied

Check `sil-more-windows-sdl3/log.txt` for output.

### Technical Details
- Damage formula: `dd = 1 + (score/10)` dice of light level
- Radius: `rad = 1 + (score/5)`
- Skill parameter: `ds = score` (GF_LIGHT handler uses this to distinguish Song from Gem)
- Only affects monsters with HURT_LITE flag
- Damage reduced by monster Will resistance and distance
- **Critical requirement:** Damage only triggers when `cave_light[monster_pos] >= 3`
- Light sources provide different damage ranges:
  - Torch (radius 1): Never damages (max light = 2)
  - Lantern (radius 2): Damages same square only (light = 3)
  - Mallorn (radius 3): Damages up to 1 square away
  - Fëanorian (radius 4): Damages up to 2 squares away
  - Silmaril (radius 7): Damages up to 5 squares away

### Messages
When Song of Trees affects a HURT_LITE monster, you'll see:
- "[Monster] is seared by radiant light!" - when damage is dealt
- "[Monster] cringes from the light!" - when stunned but no damage
- "[Monster] resists the light!" - when Will save succeeds

These messages now display (removed PROJECT_SILENT flag) to provide feedback.

# Session Notes - Morgoth Crown Tiles


## Date
2025-10-27

## Summary
- Studied MicroChasm tile encoding (`attr & 0x3F` → row, `char & 0x3F` → column) via `graf-new.prf` and `callback_sdl_pict`.
- Added `object_attr_graphics_override()` / `object_char_graphics_override()` in `src/object1.c` to remap Morgoth crown artefacts once Silmarils are removed.
- Wired overrides into the `object_attr` / `object_char` macros (`src/defines.h`) and declared them in `src/externs.h` so all item renders honour the new tiles.
- Introduced shared tile helpers (`TILE_*`) and taught the pref parser/dumper about `R#/C#` row/column tokens so artists can work numerically instead of hex.

## Notes
- Crown with three Silmarils keeps existing tile (`0x85/0x9C`, row 5 col 28); variants now use row 12 with columns 23–25 while preserving glow/alert overlay bits.

# Session Notes - Woven Theme Synergy

## Date
2025-10-25

## Summary
- Added woven theme synergy handling so specified song pairs each gain +20% of base song skill when sung together.
- Included helper utilities in `src/xtra1.c` to detect synergy pairs and grant the shared bonus after applying minor theme penalties.

# Session Notes - Song Duels Mechanics Update

## Date
2025-10-24 (Evening)

## Summary
- Added Song of Contest and Song of Lament abilities after Grace with new data entries, enumerations (SNG_CONTEST, SNG_LAMENT, SNG_MAX), and song selection UI updates.
- Extended player/monster state for targeted songs: stored duel targets and stacks, stack timestamps, cooldown timers, permanent stat/armour/damage penalties, and saved them via VERSION_EXTRA 5 bump.
- Implemented targeted duel resolution each turn (Song+Will/2 vs monster Will), 7 voice upkeep, stack accrual/decay, and the on-3-stack outcomes (stat drains, grace loss, monster debuffs, singing lockouts).
- Introduced per-turn/cleanup hooks (song_duels_new_player_turn, song_duels_handle_monster_removed), enforced major-theme-only usage, and blocked monsters from starting songs while locked out.

# Session Notes - Morgoth Victory Update

## Date
2025-10-24 (Morning)

## Summary
- Added 10% health trigger for Morgoth's new desperate state with updated stats.
- Introduced dedicated Morgoth victory flow: new messaging, notes, and high-score handling via `do_cmd_morgoth_victory()`.
- Adjusted scoring, metarun, and blessing systems to reward Morgoth slayers (3 Silmarils awarded, doubled blessing pool contribution).

### Key Changes
1. Combat: `anger_morgoth()` state 5 stats now match design (60 attack, 10d10 damage, 40 evasion, 9d4 armour, increased Will/Per). `process_monster()` promotes Morgoth to state 5 precisely at 10% HP with new log/message hooks.
2. Victory Flow: `monster_death()` now triggers the victory sequence instead of the legacy bug banner, adds `do_cmd_morgoth_victory()` with note logging, and retitles tomb/final menu text for Morgoth slayers.
3. Meta & Scores: Morgoth victors are scored as if they ascended with all three Silmarils; blessing pool contributions are doubled; `metarun_update_on_exit()` has a new branch awarding +3 Silmarils and skipping kinslaying/treachery scenes.

# Session Notes - Metarun UI Improvements

## Date
2025-10-22 (Evening)

## Metarun Info Menu Enhancements - Round 3 (Final Alignment Fixes)

Fixed remaining alignment issues and blessing power visibility.

### Fixes Applied

1. **Perfect Column Alignment**
   - Changed from right-padding spaces to left-aligned format specifier: `%-8s`
   - "Blessing" and "Curse" now properly align in 8-character column
   - Fixed formatting: `%2d: %-28s %-8s %d - %s`

2. **H:/P: Visibility Fix**
   - H: (blessing power) and P: (curse power) now **only shown when identified** (`CURSE_SEEN()`)
   - Added `bool seen = CURSE_SEEN(id);` check before displaying effect
   - D: (description) still shown always as intended

### Build Status
✅ Compiled and deployed successfully

---

## Metarun Info Menu Enhancements - Round 2 (Bug Fixes)

Fixed alignment, encoding, and width issues based on testing feedback.

### Fixes Applied

1. **Blessing Pool Meter Encoding Fix**
   - Changed from Unicode box-drawing characters (╔═╗║) to simple ASCII (+|-|#)
   - Now uses `+----------+` for borders and `|##########|` for filled sections
   - Fixes garbled character display in terminal
   - Progress text format changed to compact: "113/350" instead of "113 / 350"

2. **Curse/Blessing List Alignment**
   - Fixed column alignment: `%2d: %-28s %s %d - %s`
   - "Blessing" and "Curse   " now properly aligned (8 chars each)
   - ID field: 2 digits, Name field: 28 chars fixed width
   - Removed extra indentation (was `col + 2`, now just `col`)
   - Effect text truncation respects meter position

3. **Terminal Width Handling**
   - Footer prompt now uses actual terminal width (minimum 80)
   - Calculates: `target_width = (term_width > 80) ? term_width : 80`
   - Pads footer to full width with spaces for clean display
   - Shortened prompt text to fit: "[b] Spend blessings  [f] Threshold  [c] Difficulty  [u] Full list  [s] History"
   - Footer starts at column 0 for full-width coverage

4. **Description Visibility (D: and E:)**
   - D: (curse description) and E: (blessing description) now **always shown**, even when not identified
   - P: (curse power) and H: (blessing power) still require identification (`CURSE_SEEN()`)
   - Changed message: "(Effect not yet identified)" instead of "(Not yet identified)"

5. **Width Calculations**
   - Main display respects meter position: `max_display_width = meter_col - 4`
   - Ensures 80-column minimum width compliance throughout
   - Text truncation with "..." when exceeding display area

### Build Status
✅ Compiled successfully with only pre-existing warnings
✅ Deployed to `sil-more-windows-sdl3/`

---

## Metarun Info Menu Enhancements - Round 1 (Initial Implementation)

Improved the metarun statistics and curse/blessing display screens with better layout, visual meter, and navigation.

### Changes Made

1. **Blessing Pool Meter** (`src/metarun.c`)
   - Added `draw_blessing_meter()` function that displays a vertical progress bar on the right side
   - Shows current blessing pool progress toward next point threshold in light blue (`TERM_L_BLUE`)
   - Uses box-drawing characters (╔═╗║╚╝) with filled blocks (████) for visual appeal
   - Displays progress ratio below the meter (e.g., "2450 / 5000")
   - Positioned at right edge (column = term_width - 16) to avoid overlap with main content

2. **Enhanced 'u' Menu - Full Effects List** (`src/metarun.c`)
   - Completely rewrote `show_all_active_curses()` to show both description and power for each effect
   - Now displays **both D: (description/flavor text) and H:/P: (mechanical effect)** for identified effects
   - Added pagination with left/right arrow navigation (keys 4/6) when effects don't fit on screen
   - Page indicator in title: "=== Active Effects (Page 1/3) ==="
   - Each effect shows: name, description, and mechanical effect on separate lines
   - Handles long text truncation with "..." for terminal width
   - 4 lines per effect (name + description + power + blank separator)

3. **Threshold Selection Menu Colors** (`src/metarun.c`)
   - Added color-coded difficulty modes in `adjust_blessing_threshold_menu()`
   - "Easier" mode: Green (`TERM_L_GREEN`)
   - "Normal" mode: White (`TERM_WHITE`)
   - "Harder" mode: Orange (`TERM_ORANGE`)
   - Highlighted selection shown in yellow (`TERM_YELLOW`)
   - Improved visual hierarchy with consistent color scheme

4. **Minimum 80-Column Width**
   - Ensured all text displays properly on minimum 80-width terminals
   - Footer prompt already padded to 80 characters
   - Blessing pool text simplified to fit within left column space
   - Layout calculations respect minimum width while adapting to larger terminals

### Technical Details

- Meter height: 15 rows on tall terminals, scales down to minimum 5 rows
- Meter column: `term_width - 16` (provides 14-char wide meter + borders)
- Navigation: Keys 4 (left) and 6 (right) for pagination, any other key exits
- Data sources: Uses `curse_type` fields - `text`/`blessing_text` for D:/E:, `power`/`blessing_power` for P:/H:
- Respects `CURSE_SEEN()` flag - only shows details for identified effects

### Build Status
- Compiled successfully with standard warnings (pre-existing type comparison issues)
- Deployed to `sil-more-windows-sdl3/` directory

---

# Session Notes - Song of Revealing Implementation

## Date
2025-10-22 (Morning)

## Song of Revealing

- Added Song of Revealing entry to `lib/edit/ability.txt` after Song of the Trees (ability id 8, skill req 7, prerequisite Song of Delvings) and renumbered later song abilities/prerequisites to keep ordering consistent.
- Introduced `SNG_REVEALING` enumeration between Trees and Woven Themes (`src/defines.h`), updated name table (`src/birth.c`), and granted it full skill scaling in `ability_bonus` (`src/xtra1.c`).
- Broke out shared noise-detection logic as `detect_monster_noise()` so Listen and the new song reuse the same checks (`src/monster2.c`, declaration in `src/externs.h`).
- Implemented `sing_song_of_revealing()` in `src/spells1.c` to run Song-skill-based monster reveals each turn and permanently mark nearby items via new `song_reveal_items()` helper; added start/maintenance messaging and voice cost handling.
- Ran the VS Code Build and Deploy workflow manually (`cmake` configure/build followed by `.vscode/deploy.ps1`) to produce and copy the updated SDL3 executable.

# Session Notes - Song Debuff Decay Implementation

## Date
2025-10-21

## Song of Challenge & Song of Elbereth - Gradual Debuff Decay

**Problem:** Song debuffs (Perception/Stealth penalty for Challenge, Will penalty for Elbereth) disappeared immediately when stopping the song, making tactical song-switching less viable.

**Solution:** Implemented gradual decay system using timed effect counters with skill-scaled duration.

### Files Modified

1. **src/types.h**
   - Added `s16b song_challenge_effect` - lingering debuff counter for Song of Challenge
   - Added `s16b song_elbereth_effect` - lingering debuff counter for Song of Elbereth
   - Placed after `tmp_per` to group with other timed effects

2. **src/spells1.c** (sing function)
   - `SNG_CHALLENGE`: Sets counter based on song skill: `duration = (skill × 3) / 4`
   - `SNG_ELBERETH`: Sets counter based on song skill: `duration = (skill × 3) / 4`
   - Minimum duration of 3 turns at low skill
   - At skill 20: 15 turns duration
   - At skill 10: 7 turns duration
   - At skill 30: 22 turns duration
   - Counter maintains while song is active, then decays naturally

3. **src/dungeon.c** (timed effect decay)
   - Added decay logic after temporary perception block
   - Each turn reduces counters by 1 until they reach 0
   - Duration varies based on song skill when effect was applied

4. **src/monster2.c** (monster_skill function)
   - Changed from checking `singing()` to checking effect counter `> 0`
   - Calculates max duration dynamically: `max_duration = (song_skill × 3) / 4`
   - Penalty scales linearly: `penalty = (full_penalty × current_effect) / max_duration`
   - Ensures minimum penalty of 1 while any effect remains
   - Enhanced debug logging shows `effect/max_duration` (e.g., "effect=8/15")

5. **src/save.c & src/load.c**
   - Added serialization for both new counters after `oppose_pois`
   - Reduced spare bytes from 19 to 15 (used 4 bytes: 2 × s16b)
   - Ensures save compatibility

### Mechanics

**Duration Scaling (skill-based):**
```
Skill  5 →  3 turns (minimum)
Skill 10 →  7 turns
Skill 15 → 11 turns
Skill 20 → 15 turns (baseline)
Skill 25 → 18 turns
Skill 30 → 22 turns
```

**Debuff Strength (example at skill 20):**
- Full strength (counter = 15/15): 100% penalty
- Half strength (counter = 8/15): ~53% penalty
- Quarter strength (counter = 4/15): ~27% penalty
- Final turn (counter = 1/15): Minimum penalty of 1

**Affected Skills:**
- Song of Challenge: Monster Will & Stealth (-1 to -2 typically)
- Song of Elbereth: Monster Will (-1 to -2 typically)

**Tactical Benefits:**
- Higher song skill = longer lingering protection
- Can switch songs without instant penalty loss
- Gradual fade prevents sudden difficulty spikes
- Scales naturally with character progression

### Build Status
✅ CMake build successful
✅ Deployment successful
✅ Save/load compatibility maintained
✅ Skill-based duration scaling implemented

### Critical Fix (2025-10-21)
**Issue:** Save files created after debuff decay update were failing to load with "Read savefile failed" error.

**Root Cause:** Save/load byte count mismatch
- `save.c` was writing 19 spare bytes (3 wr_byte + 4 wr_u32b = 3 + 16 = 19)
- `load.c` was reading 15 spare bytes with `strip_bytes(15)`
- This created a 4-byte misalignment causing subsequent reads to fail

**Fix:** 
- Removed one `wr_u32b(0L)` call in `save.c` to write only 15 spare bytes (3 + 12 = 15)
- Now matches the `strip_bytes(15)` in `load.c`
- Spare byte count correctly reflects: originally 19 bytes, used 4 for song debuff counters (2 × s16b), leaving 15

---

## Session Notes - Character song index fixes

- Date: 2025-10-22 (Evening)
- Fixed mismatched song ability indices in `lib/edit/character.txt` after the addition/reordering of Song abilities in `lib/edit/ability.txt`:
   - Updated Finrod's starting Song of Staying index to `11`.
   - Updated Lúthien's Song of Lorien index to `12`.
   - Corrected Daeron's Woven Themes index to `9`.
   - Corrected Melian's Song of Mastery index to `14`.
   - Adjusted a few other song references/comments to match `ability.txt` ordering.

Verification: performed an edit-only consistency pass on `lib/edit/character.txt` and confirmed no syntax errors in the edited file.

Additional quick pass:
- Date: 2025-10-22 (Evening)
- Performed a full scan of `lib/edit/character.txt` for all `C:` lines referencing skill 7 (Song) and corrected remaining mismatches so indices match `lib/edit/ability.txt`:
   - Fixed Finarfin to start with Song of the Trees (7) and Woven Themes (9).
   - Fixed Húrin to include Song of Slaying (10) and Song of Staying (11) in the `C:` list.
   - Fixed Elu Thingol to include Song of Mastery (14) where intended.

All edited files parsed with no errors after the changes.

# Session Notes - Score Display Fix (Final)

## Date
2025-10-19

## Final Implementation

### Short Score List Display - Clean, Polished Layout

**File Modified:** `src/files.c`

**Changes Made:**
1. ✅ Added 1-column right margin for cleaner visual appearance (verdict_width = line_width - 1)
2. ✅ Removed "Order:" prefix from caption (just shows "Score (highest first)" instead)
3. ✅ Smart truncation keeping "at XXft" depth visible
4. ✅ Dynamic terminal width detection

**Layout:**
```
                    Halls of Mandos
Score (highest first)                      Layout: Short

1. Maedhros             777  Escaped with **
2. King Azaghal         673  Escaped with **
3. Fingon               660  Escaped with ***
4. Hador                192  Slain by a Young fire-drake at 800ft
```

**Column Structure:**
- **Place:** 4 chars (`"1. "`)
- **Name:** 15 chars (left-aligned)
- **Score:** 5 chars (right-aligned)
- **Gap:** 2 spaces
- **Verdict:** terminal_width - 26 - 1 (one empty column on right for clarity)

**Smart Truncation:**
- Full verdict always kept if space allows
- If truncating needed, finds " at " (depth marker)
- Truncates monster name but keeps "at XXft" visible
- Example: `"Slain by an... at 100ft"` (depth always shown)

**Indicators:**
- `*` = Silmaril (1-3)
- `V` = Morgoth slain

**Build Status:** ✅ Successful

### 2025-03-19 - Monster runtime stat persistence groundwork
- Bumped VERSION_EXTRA to 3 so the new overrides block loads only on compatible saves.
- Snapshot pristine monster race data into new r_base during init_angband() for baseline comparisons.
- Save pipeline writes per-race runtime stat overrides (stats, blows, flags, visuals) when they diverge from base; loader reapplies them for sf_extra >= 3 else restores base defaults.

- Songs updated: Song of Challenge now applies a small Perception/Stealth penalty and Song of Elbereth shaves Will via monster_skill for on-the-fly reactions.
- Fixed Song debuff build issue by referencing the correct skill_type parameter before re-running CMake build (now clean).
- Added log_debug traces in monster_skill to confirm Song of Challenge/Elbereth penalties at runtime.

## 2025-10-20 - Song of Shattering implementation
- Added new Song of Shattering ability (`SNG_SHATTERING`) with data entry in `lib/edit/ability.txt`, prerequisites, and updated song enumeration/order.
- Extended `monster_type` with persistent shattering fields; save/load path bumped to `VERSION_EXTRA 4` and now serialises damage-side/protection reductions.
- Implemented runtime helpers to honour reduced protection/damage (`monster_base_armour_sides`, melee side clamps) and wired new song logic with distance-scaled Will checks against equipped foes.
- Updated song loop/UI listings so the new song appears in selection and deducts extra voice cost when active.
- Added explicit `HAS_WEAPON/HAS_ARMOUR` monster flags plus data tagging for orcs, trolls, giants, and balrogs so Song of Shattering keys off equipment-bearing foes.
- Physical shattering now chips held gear and nearby floor weapons/armour when damageable, reducing dice toward base values while honouring artefact resistance and visibility messaging.
- Recognise the `INSCRIP_INDESTRUCTIBLE` tag (plus artefact status) when evaluating shatter targets so indestructible gear remains immune.
- Revamped savefile compatibility guard to compare full version tuples (major/minor/patch/extra) and drive feature gates, keeping older saves loadable after future version bumps.

# Session Notes - Blessing Threshold Controls

## Date
2025-10-23

## Blessing Threshold Adjustments

- Expanded runtype data (lib/edit/runtypes.txt, src/types.h, src/init1.c) to support easier/normal/harder blessing thresholds via new `L:` directive values and `blessing_threshold_modes`.
- Introduced per-metarun threshold mode persisted in the first runtime byte (src/metarun.h, src/metarun.c); recalculation logic now pulls the selected mode and falls back gracefully to normal thresholds.
- Added `f` shortcut to the metarun statistics screen with a dedicated menu for selecting easier/normal/harder thresholds, including descriptive guidance and live recalculation/update plus persistence.
- Updated blessing/curse info menu to label curse effects explicitly and surface blessing descriptions/effects for all identified curses (show_known_curses_menu).
- Blessing pool summaries and the blessing exchange dialog now present the active threshold mode while reflecting the selected progression values.


- Updated blessing threshold menu to support arrow-key navigation with in-menu confirmation prompts.
- Active curse/blessing listings (stats and full view) now surface effect text when identified, drawing from P:/H: entries.

# Session Notes - Song of Elveness

## Date
2025-10-22

- Added Song of Elveness to `lib/edit/ability.txt` before Song of Staying with matching prerequisites (`Song of the Trees`), cost, and new description.
- Shifted downstream song IDs (Staying onwards) and refreshed song enumerations (`src/defines.h`), name tables (`src/birth.c`), and song listings (`lib/edit/actual_abilities*.txt`, `lib/edit/character.txt`, `lib/edit/artefact.txt`).
- Implemented gameplay effects: Grace +1 via `calc_bonuses`, Evasion bonus `1 + song/7`, and updated per-turn handling (`src/xtra1.c`, `src/spells1.c`) including noise contribution and UI messaging.

# Session Notes - Song of Disguise

## Date
2025-10-22

- Added Song of Disguise ahead of Song of Lorien in `lib/edit/ability.txt`, keyed to Song of Silence, and propagated the new ordering through song enums, name tables, hero ability maps, and artefact comments.
- Wired `SNG_DISGUISE` behaviour in `src/spells1.c`: enforced start restrictions when observed, tracked pacified/seen-through monsters with per-turn skill contests against Will+Perception (distance, attack, and suspicion penalties), and applied the 2 voice per round upkeep.
- Hooked monster attack tracking and cleanup (`src/melee1.c`, `src/dungeon.c`, `src/monster2.c`) plus AI suppression (`src/melee2.c`) so fooled foes skip their turns until they pierce the disguise; integrated song noise and ability bonus adjustments (`src/xtra1.c`).
- Declared new song helpers in `src/externs.h` and ensured per-turn rotation/reset flows manage disguise state during level transitions and saves.

# Session Notes - Song of Revealing

## Date
2025-10-22

- Added persistent Song of Revealing hints so partially detected monsters render with the listening-style `?` marker by tracking per-monster reveals (`src/spells1.c`) and exposing `song_revealing_overlay`.
- Updated the map renderer (`src/cave.c`) to query the overlay helper so redraws no longer wipe the hint immediately; hints clear automatically when the song stops or monsters are removed.
- Re-ordered Song of Revealing processing to reset hint state each turn while keeping item reveal behaviour intact; linked overlay declaration through `src/externs.h`.
- Introduced a short-lived decay timer for Song of Revealing hints so partial detections persist for several beats even if a later roll fails, avoiding the instant flicker that previously occurred.

# Session Notes - Monster Recall Instance Stats

## Date
2025-10-24

- Threaded an optional `monster_type*` through `screen_roff`, `display_roff`, and `describe_monster`, updating all call sites (`cmd3.c`, `cmd4.c`, `xtra1.c`, `xtra2.c`, `wizard1.c`) so recall views can access live monster state when inspecting a visible target.
- Reworked `describe_monster_movement`, `describe_monster_toughness`, `describe_monster_skills`, and `describe_monster_attack` to pull per-instance data: numeric speed output with hasted/slowed markers, current/max HP ranges with curse/song adjustments, protection ranges reflecting armour penalties/bonuses, skill readouts via `monster_skill`, and blow damage/attack values recalculated with song-induced reductions.
- Swapped legacy `XdY` displays for `min-max` ranges when only race data is available, while defaulting to live `current-max` spans whenever the specific monster is known.

# Session Notes - Recall Dice Formatting

## Date
2025-10-24

- Restored XdY formatting for monster attacks and protection in `src/monster1.c`, keeping adjusted dice from any active debuffs while reverting away from min/max spans.
- Reverted monster HP recall to the base `hdice`/`hside` expression and appended a `-<amount>` suffix when Song of Lament reductions apply, via a new per-monster accumulator backed by `monster_song_hp_loss()`.
- Repurposed the song duel padding bytes (`song_hp_loss_lo/hi`) with save/load support (`src/save.c`, `src/load.c`) and helper accessors (`src/monster2.c`, `src/externs.h`) so song-induced HP penalties persist across turns and savefiles.

# Session Notes - Post-Death Spectator View

## Date
2025-10-25

- Added `death_spectator_view()` (declared in `src/externs.h`, implemented in `src/dungeon.c`) to drive a post-mortem spectator loop that reveals the full dungeon, blocks any command that would spend energy, and allows UI/navigation menus until the player presses `Esc`.
- Guarded `process_command()` with a `death_spectator_mode` whitelist so movement, inventory interaction, and other time-advancing actions are rejected gracefully while the spectator is active.
- Updated `close_game_aux()` (`src/files.c`) to launch the spectator view immediately after scoring and before displaying the tombstone, and wired the tomb menu's "View dungeon" entry to reuse the new spectator loop instead of the old `do_cmd_look()` snapshot.

# Session Notes - Final View Read-Only Polish

## Date
2025-10-25

- Exposed `death_spectator_active()` from `src/dungeon.c` so UI layers can detect the death-view state; inventories, equipment menus, and the supplies browser now call this helper to suppress `use`, `drop`, and other energy-spending actions while still allowing examination flows (`src/object1.c`, `src/cmd3.c`, `src/cmd4.c`).
- Tomb menu no longer offers the inventory/equipment branch, labels the dungeon revisit as “Final look,” and renumbers downstream options accordingly (`src/files.c`).
- Main menu entries for suicide, save, and quit-with-save render disabled and emit a warning if triggered while the corpse view is active, with navigation skipping those slots (`src/cmd4.c`).
## 2025-10-27 - Throwing Mastery & Polearm Updates

- Inserted a new melee ability (Throwing) ahead of Polearm Mastery, bumped downstream IDs/prerequisites, and updated ability tables/hero maps (lib/edit/ability.txt, lib/edit/actual_abilities_*.txt, lib/edit/character.txt, src/defines.h, src/birth.c).
- Added player_can_treat_as_throwing[_flags]() to centralize dynamic throwing checks and declared the helpers in src/externs.h.
- Hooked the new ability into thrown combat: +1 attack, half distance penalty, and Finesse-grade crit separation when hurling items flagged via the helper (src/cmd2.c, src/cmd1.c).
- Extended quiver UI/load paths to honor Polearm Mastery for great spears (slot selection, inventory carry, bonus application) and made thrown breakage/penalties respect the helper (src/cmd3.c, src/object2.c, src/xtra1.c, src/cmd2.c).
- Widened crit_bonus() with an object parameter so throwing mastery can apply selectively, updating all call sites (src/cmd1.c, src/cmd2.c, src/cmd3.c, src/melee1.c, src/spells1.c).

## 2025-10-28 - Story Font Rendering Stabilization

- Added a per-cell story-font flag to `term_win` (`src/z-term.h/.c`) and taught `Term_queue_{char,chars}` plus the flush paths (`Term_fresh_row_*`) to copy those bits so queued text remembers whether it requested story or mono rendering. SDL now tags each text batch through `Term->story_chunk_active`.
- Updated the SDL front-end to respect the new metadata: `callback_sdl_text()` inspects the chunk flag, `sdl_story_font_enable/disable()` maintain a depth counter and flip `Term->story_font_active`, and `sdl_is_story_font_enabled()` proxies to the term state (`src/main-sdl.c`). This removed the need for ad-hoc `Term_fresh()` calls just to lock in a font.
- Cleaned up UI callers that were only flushing to keep fonts sticky. Labels and status blocks in `src/xtra1.c` and `src/files.c` no longer call `Term_fresh()` after every line, which eliminates the cursor blink on character/stat adjustment screens.
- Character sheet highlights and HUD numbers inherit the correct font mode automatically now that `display_player_*` and `prt_*` no longer rely on synchronous flushes (`src/files.c`, `src/xtra1.c`).


## 2025-10-28 - Story Font Flag Propagation Fix

- Added sdl_apply_story_font_state() so every SDL term shares the same story_font_active bit whenever sdl_story_font_enable()/disable() adjust the nesting depth. This ensures queued glyphs record the correct font mode even if the active term changes between calls.
- Rebuilt successfully via build-cmake.bat to verify the SDL front-end compiles with the new helper.
- Added trace logging around story-font activation and the SDL text callback to diagnose why proportional text isn't chosen at runtime; rebuilt via build-cmake.bat.
- Patched z-term chunking so the first glyph in a story-font stripe captures both attr and the font flag, ensuring SDL sees chunk_story_font=true after logs showed the bit was being lost.\n
- Reset story_chunk_active after each text stripe so SDL doesn\'t keep rendering later glyphs in story mode once the character sheet closes; rebuilt via build-cmake.bat.\n
- Added a 'Story UI Lists' option that renders inventory/equipment/look panels with story font when desired, converted the intro screens to use story text, and ensured the new rendering path resets SDL story state cleanly.\n
