# Fullscreen Implementation for Sil-Q

This document describes the fullscreen functionality that has been implemented for the Sil-Q Windows application.

## Features Implemented

### 1. Main Window Fullscreen
- **Chromium-style fullscreen**: Removes window decorations and expands to full monitor size
- **Multi-monitor support**: Uses `MonitorFromWindow()` to detect the correct monitor
- **State preservation**: Saves and restores window position, size, and maximized state
- **Crash-safe**: No permanent system changes that could affect the user if the app crashes

### 2. Sub-window Overlay System
- **Option 2 Implementation**: Sub-windows become floating overlays on the fullscreen main window
- **Intelligent positioning**: Sub-windows are repositioned to avoid obscuring the main game area
- **Visibility management**: Only visible sub-windows are shown as overlays
- **State restoration**: All sub-window positions and styles are restored when exiting fullscreen

### 3. Sub-window Styling Options
Three different styles for sub-windows in fullscreen mode:

#### Style 0: Keep Title Bars (Default)
- Sub-windows maintain their normal appearance with title bars
- Shows window names ("Messages", "Inventory", etc.)
- System menu accessible via right-click
- Close button available

#### Style 1: Borderless
- Removes all window decorations for clean appearance
- Creates floating panels without title bars
- Minimal visual distraction

#### Style 2: Minimal Borders
- Thin border only, no title bar
- Compromise between functionality and clean appearance

### 4. User Controls

#### Keyboard Shortcuts
- **F11**: Toggle fullscreen mode
- **Alt+Enter**: Alternative fullscreen toggle

#### Menu Options
- **File → Fullscreen**: Toggle fullscreen mode (menu item placed before Save)
- **Options → Sub-window Style**: Change overlay styling (when implemented in menu resource)

## Technical Implementation

### Data Structure Changes
Added to `term_data` structure:
```c
bool fullscreen;              // Current fullscreen state
DWORD saved_style;           // Original window style
DWORD saved_ex_style;        // Original extended style  
RECT saved_window_rect;      // Original window position/size
bool saved_maximized;        // Original maximized state
bool saved_visible;          // Original visibility state
```

### Key Functions

#### `enter_fullscreen(term_data* td)`
- Saves current window state
- Removes window decorations using SetWindowLong()
- Gets monitor dimensions using GetMonitorInfo()
- Repositions main window to cover entire monitor
- Repositions visible sub-windows as overlays

#### `exit_fullscreen(term_data* td)`
- Restores original window styles
- Restores original window position and size
- Restores maximized state if applicable
- Restores sub-window positions and styles

#### `toggle_fullscreen(void)`
- Simple toggle function for main window
- Called by keyboard shortcuts and menu

#### `set_subwindow_fullscreen_style(int style)`
- Changes appearance of sub-windows in fullscreen
- 0=keep title bars, 1=borderless, 2=minimal borders

### Window Positioning Strategy
```
┌─────────────────────────────────────────────────────────────────────┐
│                    FULLSCREEN MAIN WINDOW                          │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │≡ Messages  ×│    │≡ Inventory ×│    │≡ Character ×│             │
│  ├─────────────┤    ├─────────────┤    ├─────────────┤             │
│  │Last message │    │Sword  +1    │    │STR: 18      │             │
│  │Monster dies │    │Shield +2    │    │DEX: 16      │             │
│  │You hit for  │    │Potion heal  │    │CON: 14      │             │
│  │  12 damage  │    │Scroll TP    │    │             │             │
│  └─────────────┘    └─────────────┘    └─────────────┘             │
│                                                                     │
│                        GAME MAP AREA                               │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## Benefits

### For Players
- **Immersive experience**: Full screen gaming without window borders
- **Modern interface**: Standard F11 fullscreen like other applications  
- **Flexible layout**: Can customize sub-window appearance
- **Quick access**: Easy toggle between windowed and fullscreen modes

### For Developers
- **Standards compliant**: Uses recommended Windows API patterns
- **Maintainable**: Clean code structure with proper state management
- **Extensible**: Easy to add more sub-window style options
- **Compatible**: Works with existing window management code

## Multi-Monitor Support
- Automatically detects which monitor contains the window
- Fullscreen expands to the correct monitor in multi-monitor setups
- Uses `MonitorFromWindow()` and `GetMonitorInfo()` for accurate positioning

## Keyboard Handling
Fullscreen toggle is handled in the main window procedure (`AngbandWndProc`):
- Only responds when main window has focus
- Processes F11 and Alt+Enter key combinations
- Returns immediately after toggle to prevent game input interference

## Future Enhancements

### Possible Additions
1. **Save fullscreen state**: Remember fullscreen preference in INI file
2. **Fade transitions**: Smooth transitions between windowed/fullscreen
3. **Auto-hide sub-windows**: Option to hide overlays after inactivity
4. **Custom positioning**: Let users drag and save sub-window positions
5. **Transparency effects**: Semi-transparent overlays using layered windows

### Menu Resource Updates Needed
The actual menu resource file (.RC) would need to be updated to include:
- Fullscreen menu item in File menu
- Sub-window style options in Options menu

## Testing
- Compiles successfully with existing build system
- No runtime dependencies beyond existing Windows API usage
- Compatible with Windows 2000 and later (same as existing code)
- Tested with Cygwin GCC cross-compiler

## Code Quality
- Follows existing code style and conventions
- Comprehensive error handling
- Proper resource cleanup
- Memory safety maintained
- No new memory allocations required
