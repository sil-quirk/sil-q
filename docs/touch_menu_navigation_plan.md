# Touch and Mouse Menu Navigation Plan

This plan targets the current checkout. Line numbers are exact for this tree and should be rechecked with `rg -n` before patching if nearby code has moved.

## Goal

Make every menu navigable with touch controls and desktop mouse without requiring keyboard input. The result should feel intentional on touch screens:

- Tap a row to focus it.
- Tap the focused row again, or tap an explicit footer action, to activate it when activation has consequences.
- Right-click or long-press opens the secondary action where the menu already has one, usually recall/details/inspect.
- Full row hit targets are used, not only the rendered text.
- Modal "press any key" screens can be dismissed by tapping the prompt or the full modal area.
- Mouse and touch use the same safe press/release behavior, so accidental down events do not immediately activate commands.
- Touch settings are split into Touch Panel for the on-screen button panel and Touch Control for menu touch commands, map touch movement, and swipe gestures.
- Every menu that uses registered touch commands must respect its Touch Control menu category setting: Inventory/Equipped, Supply, or Others.
- Touch Movement must support On, Off, and Long click only; Long click only disables tap-to-follow movement and uses a long press on the map to start movement.

## Existing Infrastructure

The repo already has a low-level click registry that converts SDL pointer coordinates into terminal cells and menu choices.

- `src/externs.h:1543-1554` declares `get_menu_choice()`, `ui_menu_click_*()`, `UI_MENU_CLICK_PRIMARY`, `UI_MENU_CLICK_SECONDARY`, and `UI_MENU_CLICK_WAKE_KEY`.
- `src/util.c:25` defines `UI_MENU_CLICK_MAX_ENTRIES` as `96`.
- `src/util.c:51-58` clears/begins a clickable menu.
- `src/util.c:60-95` registers a clickable row or cell range.
- `src/util.c:122-136` stores the chosen cell action.
- `src/util.c:143-167` exposes `ui_menu_click_take_action()` and `ui_menu_click_take()`.
- `src/main-sdl.c:3300-3314` converts pointer coordinates to terminal cells and sends Enter or `UI_MENU_CLICK_WAKE_KEY`.
- `src/main-sdl.c:3397-3519` implements touch press, motion cancellation, release, and long-press.
- `src/main-sdl.c:5800-5914` routes SDL mouse and finger events.

Keep this infrastructure. Most work should be registering hitboxes in existing menu loops and consuming `ui_menu_click_take_action()` immediately after `inkey()`.

## Foundation Changes

### 1. Increase click registry capacity

Patch `src/util.c:25`:

```c
#define UI_MENU_CLICK_MAX_ENTRIES 256
```

Why: knowledge browser pages, run history pages, and mixed group/list/detail screens can exceed 96 clickable regions when rows, tabs, and footer actions are all registered.

### 2. Add small helper wrappers

Add declarations near `src/externs.h:1543-1554`:

```c
extern void ui_menu_click_add_full_row(int choice, int row);
extern void ui_menu_click_add_span(int choice, int col, int row, int end_col);
```

Add implementations after `src/util.c:60-95`:

```c
void ui_menu_click_add_full_row(int choice, int row)
{
    ui_menu_click_add(choice, 0, row, Term->wid);
}

void ui_menu_click_add_span(int choice, int col, int row, int end_col)
{
    int width = end_col - col;

    if (width < 1) width = 1;
    ui_menu_click_add(choice, col, row, width);
}
```

Use these helpers in new code so row targets are consistent and easy to audit.

### 3. Make mouse use press/release semantics

Current desktop mouse activates on mouse down. Touch waits for release on the same cell. Align mouse with touch.

Patch `src/main-sdl.c:376-384`:

- Add a `bool mouse;` field to `menu_touch_press_state`.

Patch `src/main-sdl.c:3397-3519`:

- Extend `sdl_menu_touch_handle_pointer_down()`, `sdl_menu_touch_handle_pointer_motion()`, and `sdl_menu_touch_handle_pointer_up()` to accept `bool mouse`.
- Store `mouse` in `g_menu_touch_press`.
- Keep movement cancellation and same-cell release checks identical for mouse and touch.
- Keep long-press only for touch. Desktop mouse already has right-click for secondary action.

Patch `src/main-sdl.c:5814-5815`:

- Replace direct `sdl_main_screen_handle_menu_text_pointer(..., UI_MENU_CLICK_PRIMARY)` on left mouse down with `sdl_menu_touch_handle_pointer_down(..., true)`.

Patch `src/main-sdl.c:5849-5854`:

- On left mouse up, call `sdl_menu_touch_handle_pointer_up(..., true)`.
- Leave right mouse up as secondary action.

Patch the mouse motion branch near `src/main-sdl.c:5800-5914`:

- When a menu press is active, call `sdl_menu_touch_handle_pointer_motion(..., true)` to cancel if the pointer leaves the original cell.

Patch finger calls at `src/main-sdl.c:5872`, `src/main-sdl.c:5897`, and `src/main-sdl.c:5913` for the new function signatures with `mouse=false`.

## Shared Interaction Pattern

Use this pattern in each menu loop:

1. Call `ui_menu_click_begin()` after `Term_clear()` or before drawing clickable rows.
2. Register each visible item after it is drawn.
3. Register footer actions such as Back, Confirm, Next Page, Previous Page, Sort, and Recall.
4. Read input with the existing `inkey()`.
5. Immediately call `ui_menu_click_take_action(&choice, &action)`.
6. For `UI_MENU_CLICK_PRIMARY`, focus non-current rows and activate only if the row was already current or the menu is a one-step command menu.
7. For `UI_MENU_CLICK_SECONDARY`, run the menu's inspect/recall/details path if one exists. If not, use the same behavior as primary on the focused row.
8. Call `ui_menu_click_clear()` before returning, before `screen_load()`, and when leaving a modal.

Recommended local IDs for footer and tab actions:

```c
enum {
    MENU_CLICK_BACK = -1,
    MENU_CLICK_CONFIRM = -2,
    MENU_CLICK_PREV_PAGE = -3,
    MENU_CLICK_NEXT_PAGE = -4,
    MENU_CLICK_RECALL = -5,
    MENU_CLICK_SORT = -6,
    MENU_CLICK_TAB_BASE = 100000,
    MENU_CLICK_GROUP_BASE = 200000,
    MENU_CLICK_ENTRY_BASE = 300000
};
```

Keep IDs local to the file or function unless multiple files need the same constants.

## Menus Already Partly Covered

These already use the click registry. They need only hardening, full-row hitboxes, and the shared mouse press/release change.

### Main command menu

- Function: `src/cmd4.c:12139` `main_menu_aux()`.
- Click setup: `src/cmd4.c:12182`.
- Row registration: `src/cmd4.c:12195`.
- Click consumption: `src/cmd4.c:12240`.

Implementation notes:

- Change row registration to full available width.
- Add footer click targets for close/back if shown.
- Keep tap activation immediate because this is a command menu.

### Options and settings menus

- `src/cmd4.c:17993-18045` `legacy_options_menu()`.
- `src/cmd4.c:18235-18317` `options_menu()`.
- `src/cmd4.c:18435-18467` `input_options_menu()`.
- `src/cmd4.c:14994-15347` option page loop.
- `src/cmd4.c:16210-16425` pane settings.
- `src/cmd4.c` Touch Panel settings.
- `src/cmd4.c` Touch Control settings.
- `src/cmd4.c:19117-19214` keybind settings.
- `src/cmd4.c:20640-20724` controller settings.

Implementation notes:

- Convert narrow text-width registrations to full-row or full-control registrations.
- For toggles and sliders, tapping the setting row should focus it; tapping focused row should cycle/increment. Right-click or long-press should cycle backward where keyboard already supports reverse cycling.
- Add footer targets for Back, Restore Defaults, and Next/Previous Page where those commands exist.
- Keep swipe gesture controls under Touch Control, not Touch Panel.
- New touch-aware menus must use the shared click registry and set a category when they belong to Inventory/Equipped or Supply; untagged menus are controlled by Others.

### Ability, oath, and bane menus in gameplay

- Bane menu click setup: `src/cmd4.c:2608`; row registration: `src/cmd4.c:2630`; click consumption: `src/cmd4.c:2711`.
- Gameplay oath menu click setup: `src/cmd4.c:2959`; row registration: `src/cmd4.c:2988`; click consumption: `src/cmd4.c:3082`.
- Ability skill rows: `src/cmd4.c:3175`; click setup: `src/cmd4.c:3191`; click consumption: `src/cmd4.c:3208`.
- Ability list click setup: `src/cmd4.c:3288`; row registration: `src/cmd4.c:3491`; click consumption: `src/cmd4.c:3817`.

Implementation notes:

- Preserve current behavior where a first tap highlights and a second tap/confirm buys or selects.
- Right-click/long-press should show details where the keyboard path already has a details action.
- Add footer click targets for Escape/Back and Confirm/Select.

### Enhanced inventory and equipment menus

- Inventory state globals: `src/object1.c:23-28`.
- Inventory setter helpers: `src/object1.c:53` and `src/object1.c:204`.
- `get_item()` begins at `src/object1.c:5234`.
- Inventory visible list setup: `src/object1.c:5457-5552`.
- Main item input loop: `src/object1.c:5732`.
- Enhanced inventory click setup: `src/object1.c:6828`; row registration: `src/object1.c:7059`; click consumption: `src/object1.c:7354`; clear: `src/object1.c:7640`.
- Enhanced equipment click setup: `src/object1.c:7798`; row registration: `src/object1.c:7982`; click consumption: `src/object1.c:7993`; clear: `src/object1.c:8224`.

Implementation notes:

- Keep the existing inventory/equipment overlay layout math.
- Full row target should include the icon/letter, item name, and weight/value columns.
- Tap row focuses. Tap focused row selects. Long-press/right-click opens inspect/recall where available.
- The floor item row must keep the `-)` prefix and the `-` shortcut.
- Register footer/tab targets for Inventory, Equipment, Floor, Supplies, Back, and Confirm if they are visible.

### Supplies menu

- Function: `src/cmd4.c:27410` `do_cmd_knowledge_supplies()`.
- Click setup: `src/cmd4.c:27637`.
- Group registration: `src/cmd4.c:27654`.
- Entry registration: `src/cmd4.c:27671`.
- Click consumption: `src/cmd4.c:27742`.
- Primary/secondary mapping: `src/cmd4.c:27752`.

Implementation notes:

- Keep primary tap as use/select and secondary as recall.
- Add full-column hitboxes for group and entry columns.
- Add footer targets for Back and Recall.

### Unified look sidebar

- Click wake handling in look command: `src/cmd3.c:4460-4475`.
- Primary click handling: `src/cmd3.c:5089-5102`.
- Compact sidebar setup: `src/cmd4.c:29385`; row registration: `src/cmd4.c:29415`.
- Full sidebar setup: `src/cmd4.c:29520`; monster registration: `src/cmd4.c:29693`; object registration: `src/cmd4.c:29885`.

Implementation notes:

- Keep primary tap as focus/look target.
- Keep secondary as details where available.
- Full row hitboxes should not overlap the map area.

## Menus To Patch

### Generic letter choice helper

- Function: `src/util.c:3999-4110` `get_menu_choice()`.
- Caller found at `src/cmd1.c:4393`.
- Current row draw loop: `src/util.c:4014-4036`.
- Input read: `src/util.c:4038`.
- Return: `src/util.c:4109`.

Patch:

- Call `ui_menu_click_begin()` before the row draw loop.
- After each row is printed at `src/util.c:4033-4036`, register `choice=i` on `row=i + 1`.
- After `ch = inkey();` at `src/util.c:4038`, consume `ui_menu_click_take_action(&choice, &action)`.
- Primary or secondary click should set `choice=clicked` and finish.
- Call `ui_menu_click_clear()` before `return choice;`.

### Birth character choice lists

- Function: `src/birth.c:1617-1970` `get_player_choice()`.
- Visible row loop: `src/birth.c:1661-1690`.
- Input read: `src/birth.c:1818`.
- Full description modal key wait: `src/birth.c:1609`.

Patch:

- Call `ui_menu_click_begin()` before the visible row loop.
- After the row text is printed at `src/birth.c:1690`, register `choice=i + top` using the full list width.
- After `inkey()` at `src/birth.c:1818`, consume clicks.
- Primary tap on a non-current row sets `cur` and adjusts `top`.
- Primary tap on the current row selects it, unless the row is a ghost/disabled entry.
- Secondary click opens the full description screen if `allow_full_description_screen` is true. Otherwise it behaves like primary.
- In `display_character_description_screen()`, add a full-screen or prompt-row click target before the wait at `src/birth.c:1609`.

### Birth blitz setup

- Draw function: `src/birth.c:3170` `blitz_setup_draw()`.
- Options drawn: `src/birth.c:3185-3198`.
- Loop begins: `src/birth.c:3230`.
- Input read: `src/birth.c:3235`.

Patch:

- In `blitz_setup_draw()`, call `ui_menu_click_begin()` after `Term_clear()` at `src/birth.c:3178`.
- Register the option rows at `src/birth.c:3185-3198`.
- Add footer targets for Back and Begin where those prompts are drawn.
- After input at `src/birth.c:3235`, consume clicks.
- Primary tap on a non-current option focuses it. Primary tap on the current option starts/selects. Secondary behaves as current-row confirm.

### Birth oath selection

- Function: `src/birth.c:3904-4223` `select_oath()`.
- Clear screen: `src/birth.c:3962`.
- Compact list rows: `src/birth.c:3976-3992`.
- Compact footer prompts: `src/birth.c:4010`, `src/birth.c:4015`, `src/birth.c:4065`, `src/birth.c:4070`.
- Wide/detail list rows: `src/birth.c:4094-4110`.
- Wide/detail footer prompts: `src/birth.c:4139`, `src/birth.c:4144`.
- Input read: `src/birth.c:4149`.

Patch:

- Call `ui_menu_click_begin()` after `Term_clear()` at `src/birth.c:3962`.
- Register each visible oath row in both compact and wide modes.
- Register the compact page indicator/detail toggle as a click target.
- Primary tap on a non-current oath focuses it.
- Primary tap on the current oath selects it in wide mode, or opens the detail page in compact mode.
- Secondary click selects the focused oath in wide mode and opens details in compact mode.
- Add footer targets for Back, Detail/List, and Select.

### Birth blitz effect list

- Function: `src/birth.c:4443` `blitz_select_effect_from_list()`.
- Clear screen: `src/birth.c:4473`.
- Row draw loop: `src/birth.c:4479-4488`.
- Input read: `src/birth.c:4537`.

Patch:

- Call `ui_menu_click_begin()` after `Term_clear()` at `src/birth.c:4473`.
- Register each visible row after `src/birth.c:4487`.
- Primary tap on non-current row focuses it.
- Primary tap on current row, or secondary tap on any enabled row, selects it.
- Add Back and Select footer targets.

### Birth stat allocation

- Stat draw area: `src/birth.c:4990-5184`.
- Stat rows: `src/birth.c:5062-5103`.
- Input read: `src/birth.c:5138`.
- Navigation/increment handling: `src/birth.c:5161-5183`.

Patch:

- Call `ui_menu_click_begin()` before drawing stats each loop.
- Register each stat row after `src/birth.c:5103`.
- Add explicit `-` and `+` hitboxes around each stat's decrement/increment affordance. If there is no visible affordance yet, add compact `-` and `+` text beside the value column.
- Primary tap on a row focuses it.
- Primary tap on `+` increments the stat. Primary tap on `-` decrements it.
- Secondary tap on the row decrements the focused stat, matching reverse keyboard behavior.
- Add footer targets for Back and Continue.

### Birth skill allocation

- Skill allocation draw area: `src/birth.c:5316-5353`.
- Input read: `src/birth.c:5388`.
- Navigation/increment handling: `src/birth.c:5430-5458`.

Patch:

- Use the same pattern as stat allocation.
- Register every visible skill row after it is drawn.
- Add `-` and `+` hitboxes beside skill values.
- Row tap focuses. `+` and `-` directly change investment.
- Add footer targets for Back and Continue.

### Knowledge top menu

- Function: `src/cmd4.c:28044-28139` `do_cmd_knowledge()`.
- Clear screen: `src/cmd4.c:28065`.
- Choice rows: `src/cmd4.c:28071-28078`.
- Input read: `src/cmd4.c:28084`.

Patch:

- Call `ui_menu_click_begin()` after `Term_clear()` at `src/cmd4.c:28065`.
- Register choices 1 through 6 on rows 4 through 9.
- After input at `src/cmd4.c:28084`, map a primary or secondary click to `ch = I2D(choice)`.
- Clear click state before leaving the function.

### Knowledge browser pages

- Main function: `src/cmd4.c:26794` `do_cmd_knowledge_browser_page()`.
- Tab drawing: `src/cmd4.c:25845-25877` `knowledge_draw_tabs()`.
- Group drawing: `src/cmd4.c:26040-26058` `knowledge_display_groups()`.
- Artifact list drawing: `src/cmd4.c:26276-26320`.
- Object list drawing: `src/cmd4.c:26323-26398`.
- Monster list drawing: `src/cmd4.c:26448-26507`.
- Curse list drawing: `src/cmd4.c:26555-26576`.
- Artifact page input: `src/cmd4.c:26965`.
- Object page input: `src/cmd4.c:27114`.
- Monster page input: `src/cmd4.c:27253`.
- Curse page input: `src/cmd4.c:27337`.

Patch:

- Call `ui_menu_click_begin()` once per redraw after `knowledge_draw_frame()`.
- Register tabs inside `knowledge_draw_tabs()` using `MENU_CLICK_TAB_BASE + page`.
- Register visible groups inside or immediately after `knowledge_display_groups()` using `MENU_CLICK_GROUP_BASE + group_index`.
- Register visible entries in each list drawing function using `MENU_CLICK_ENTRY_BASE + entry_index`.
- Register footer actions for Back, Previous Page, Next Page, and Recall/Details when visible.
- Primary tab click switches page.
- Primary group click focuses the group and resets entry focus.
- Primary entry click focuses the entry. If it was already focused, open recall/details.
- Secondary entry click opens recall/details immediately.
- Footer Previous/Next should page the list, not change selection unexpectedly.

### Score pages

- Function: `src/score/score_ui.c:1032` `display_scores_pages()`.
- Score row draw loop: `src/score/score_ui.c:1123-1137`.
- Input read: `src/score/score_ui.c:1166`.

Patch:

- Call `ui_menu_click_begin()` before drawing the current page.
- Register visible score rows for focus only, even if no detail view exists yet.
- Register footer targets for Exit, Previous Page, Next Page, and Order/Layout if those commands are visible.
- After input at `src/score/score_ui.c:1166`, consume click actions before keyboard switch logic.

### Run history list

- Function: `src/score/score_ui.c:1682` `do_cmd_run_history()`.
- Clear screen: `src/score/score_ui.c:1727`.
- Visible run rows: `src/score/score_ui.c:1866-1938`.
- Input read: `src/score/score_ui.c:1952`.

Patch:

- Call `ui_menu_click_begin()` after `Term_clear()` at `src/score/score_ui.c:1727`.
- Register each visible run row after it is printed at `src/score/score_ui.c:1937`.
- Primary tap on a non-current run focuses it.
- Primary tap on the current run opens details.
- Secondary tap opens details immediately.
- Register footer targets for Back, Previous Page, Next Page, Sort, and Filter if visible.

### Run history detail

- Function: `src/score/score_ui.c:2745` `run_history_show_detail()`.
- Panel tabs are drawn at `src/score/score_ui.c:2818`.
- Input read: `src/score/score_ui.c:2905`.
- Abilities panel draw function: `src/score/score_ui.c:2482`.
- Milestones panel draw function: `src/score/score_ui.c:2528`.
- Artefact panel draw function: `src/score/score_ui.c:2608`.
- Monster panel draw function: `src/score/score_ui.c:2660`.

Patch:

- Add click registration to `run_history_draw_panel_tabs()` and call it from the draw path at `src/score/score_ui.c:2818`.
- Register list rows in each panel draw function.
- Primary tab click switches panels.
- Primary row click focuses details if the panel has row-level details.
- Secondary row click opens inspect/recall for artefact and monster panels.
- Register Back, Previous Run, Next Run, and Sort footer actions if visible.

### Smithing menus

Patch every smithing menu loop in `src/cmd4.c` that currently depends on keyboard letters/arrows only.

Primary targets:

- Song selector input: `src/cmd4.c:1996`; list draw call: `src/cmd4.c:1942`.
- `create_sval_menu_aux()` row labels: `src/cmd4.c:7724`; highlight: `src/cmd4.c:7736`; input: `src/cmd4.c:7757`; choose/back/nav: `src/cmd4.c:7772-7793`.
- `create_tval_menu_aux()` row labels: `src/cmd4.c:7891`; input: `src/cmd4.c:7908`; letter selection: `src/cmd4.c:7911`.
- Smithing option/alloy area input: `src/cmd4.c:8362`.
- Additional smithing-related input loops: `src/cmd4.c:8771`, `src/cmd4.c:9101`, `src/cmd4.c:9304`, `src/cmd4.c:9794`, `src/cmd4.c:10224`, `src/cmd4.c:10498`, `src/cmd4.c:10698`, `src/cmd4.c:11118`.

Patch:

- In each loop, call `ui_menu_click_begin()` before drawing rows.
- Register each visible craft/category/material/modifier row immediately after drawing it.
- Primary tap on non-current row focuses it.
- Primary tap on current row selects it.
- Secondary tap selects current row or opens details if the menu has a description/recall path.
- Register Back, Confirm, Previous Page, Next Page, and Clear/Reset footer actions wherever they are shown.
- For numeric allocation menus, add explicit `-` and `+` hitboxes rather than relying on row tap cycling.

### Song selection

- Song selector list draw call: `src/cmd4.c:1942` `show_songs_with_highlight(highlight)`.
- Input read: `src/cmd4.c:1996`.

Patch:

- Add row registration inside `show_songs_with_highlight()`.
- After input at `src/cmd4.c:1996`, consume click actions.
- Primary tap focuses a non-current song.
- Primary tap on current song selects it.
- Secondary tap opens description if available; otherwise it selects.
- Add footer targets for Back and Sing/Select.

### Cleanse and sanctity item menu

- File: `src/cmd6.c`.
- Selection function body: `src/cmd6.c:230-425`.
- Screen save: `src/cmd6.c:247`.
- Clear screen: `src/cmd6.c:264`.
- Row drawing: `src/cmd6.c:271-321`.
- Input read: `src/cmd6.c:360`.

Patch:

- Call `ui_menu_click_begin()` after `Term_clear()` at `src/cmd6.c:264`.
- Register each visible item row after it is drawn.
- Primary tap on non-current item focuses it.
- Primary tap on current item selects it.
- Secondary tap inspects the item if an inspect path exists; otherwise selects.
- Register Back/Cancel and Confirm footer targets.
- Clear clicks before `screen_load()`.

### Thrall quest reward menu

- Function area: `src/thrall_quest.c:1500-1680`.
- Reward options drawn: `src/thrall_quest.c:1532` and `src/thrall_quest.c:1536`.
- Prompt rows: `src/thrall_quest.c:1563` and `src/thrall_quest.c:1567`.
- Input read: `src/thrall_quest.c:1575`.

Patch:

- Call `ui_menu_click_begin()` before drawing reward options.
- Register each reward row.
- Primary tap focuses non-current reward and selects current reward.
- Secondary tap selects the tapped reward.
- Register Back/Cancel if that path exists.

### Quest typewriter/story prompts

- File: `src/xtra2.c`.
- Typewriter menu starts around `src/xtra2.c:8103`.
- Skip/continue input checks occur around `src/xtra2.c:8202` and `src/xtra2.c:8276`.

Patch:

- For the active typewriter phase, a tap anywhere in the text panel should skip the animation and reveal the full text.
- For the final prompt, register the prompt row or full panel as Continue.
- Use the same registered click path so `UI_MENU_CLICK_WAKE_KEY` can wake blocked waits.

### Varda reward choice menu

- Function: `src/xtra2.c:8882-9007` `prompt_varda_reward_choice_menu()`.
- Choice rows drawn: `src/xtra2.c:8931-8943`.
- Input read: `src/xtra2.c:8976`.

Patch:

- Call `ui_menu_click_begin()` before drawing choices.
- Register each visible reward row.
- Primary tap on non-current reward focuses it.
- Primary tap on current reward confirms it.
- Secondary tap opens the existing description/recall path, matching `r`.
- Add footer targets for Back, Confirm, and Recall.

### Metarun menus

File: `src/metarun.c`.

Primary targets:

- Curse choice function: `src/metarun.c:1720-1948`; input: `src/metarun.c:1914`.
- Remove curse/blessing choice area: `src/metarun.c:3297-3408`; input: `src/metarun.c:3373`.
- Minor blessing choice area: `src/metarun.c:3558-3646`; input: `src/metarun.c:3614`.
- Major blessing choice area: `src/metarun.c:3725-3883`; input: `src/metarun.c:3810`.
- Metarun action menu area: `src/metarun.c:3893-4052`; input: `src/metarun.c:4007`.
- Blessing threshold menu starts around `src/metarun.c:4428`; input: `src/metarun.c:4521`.
- Metarun main action menu input: `src/metarun.c:5135`.
- Difficulty choice menu starts around `src/metarun.c:5282-5552`; input: `src/metarun.c:5399`.

Patch:

- Add `ui_menu_click_begin()` to each draw loop.
- Register each visible curse, blessing, action, and difficulty row.
- Primary tap focuses non-current row and confirms current row.
- Secondary tap opens details where available; otherwise confirms.
- Register Back, Confirm, Previous Page, Next Page, and Adjust/Reset footer actions where shown.
- Be careful not to change metarun state until the same point where keyboard confirmation currently mutates state.

## Modal and Prompt Coverage

Several screens are not menus but still block on `inkey()` and must be dismissible by touch.

Patch pattern:

- Before the blocking `inkey()`, call `ui_menu_click_begin()`.
- Register the prompt row or the main text panel as `MENU_CLICK_CONFIRM`.
- After `inkey()`, consume the click action and treat it as Enter/continue.
- Clear click state before returning or restoring the screen.

High-priority prompts:

- `src/birth.c:1609` character description.
- `src/xtra2.c:8276` typewriter continue.
- Any modal reached from knowledge recall/details if it waits for a key and does not already use the generic any-key prompt text.

## User-Friendly Hitbox Rules

- Minimum target: one full terminal row high and the full visual column width.
- For two-column screens, row hitboxes must be constrained to their own column and must not overlap.
- For destructive or irreversible choices, first tap focuses and second tap confirms.
- For harmless navigation choices, one tap activates.
- For scrollable lists, tapping Previous/Next Page must not also select the row underneath.
- Register footer controls after row controls only if the footer visually overlaps rows. Otherwise order does not matter.
- Avoid hidden touch-only gestures. Anything required for touch should have visible text, a tab label, a footer command, or a `-`/`+` affordance.
- Long-press should never be the only way to proceed. It is for secondary convenience only.

## Implementation Order

1. Patch shared infrastructure in `src/util.c`, `src/externs.h`, and `src/main-sdl.c`.
2. Harden already-covered menus by changing narrow hitboxes to full-row hitboxes.
3. Patch the generic `get_menu_choice()` helper.
4. Patch birth/character creation menus, because these are early user-facing flows.
5. Patch knowledge, score, run history, inventory/equipment, and supplies.
6. Patch smithing, song, quest/reward, cleanse/sanctity, and metarun menus.
7. Patch modal prompt waits.
8. Run a full build and manual smoke test.

## Validation Checklist

Build:

- Run `.\build-cmake.bat`.

Mouse smoke tests:

- Left mouse down on a row, drag away, release: nothing activates.
- Left click a command menu row: command activates.
- Left click a browser row once: focus changes.
- Left click the focused browser row again: details/select activates.
- Right click a row with recall/details: details opens.
- Footer Back/Next/Previous/Confirm controls work without keyboard.

Touch smoke tests:

- Tap row: focus or activate according to menu type.
- Long-press row: secondary action where available.
- Tap and drag off row: canceled.
- Tap modal prompt: continues.
- Tap `-` and `+` controls in stat/skill/numeric menus: values change predictably.

Regression smoke tests:

- Keyboard navigation still works in every patched menu.
- Inventory/equipment overlays keep their current layout.
- Floor items still show the `-)` prefix and respond to the `-` shortcut.
- Unified look still highlights map targets correctly.
- Combat roll overlay is not cleared or shifted by menu clicks.
- `screen_save()`/`screen_load()` overlays do not leave stale click regions behind.
