# SDL Configuration for Sil-More (JSON Format)

## Configuration File: `sil_sdl.json`

The SDL3 build of Sil-More uses a JSON file for configuration. This file is automatically created with default values on first run and is saved/updated when you exit the game.

### Location

- **Standalone builds**: `sil_sdl.json` in the game directory
- **Deployment builds**: `sil-more-windows-sdl3/sil_sdl.json`

### Command-Line Arguments

Command-line arguments override JSON file settings:

```bash
# Scale the main view (integer multiplier)
sil-more.exe --scale 2

# Enable/disable tiles
sil-more.exe --tiles      # Use graphical tiles (default)
sil-more.exe --ascii      # Use ASCII characters

# Fullscreen vs windowed mode
sil-more.exe --fullscreen # Fullscreen mode (default)
sil-more.exe --windowed   # Windowed mode

# Set auxiliary view font size
sil-more.exe --font-size 20

# Set margin around panes
sil-more.exe --margin 8
```

### JSON Structure

#### `sdl` Object

Controls basic SDL settings:

```json
{
  "sdl": {
    "mainViewScale": 1,
    "auxViewFontSize": 18,
    "margin": 4,
    "fullscreen": true,
    "tiles": true
  }
}
```

**Fields:**
- **mainViewScale** (integer, default: 1): Scaling factor for the main view. Higher values = larger tiles but fewer visible.
- **auxViewFontSize** (integer, default: 18): Font size in points for auxiliary panes (inventory, log, etc.).
- **margin** (integer, default: 4): Margin in pixels around each pane.
- **fullscreen** (boolean, default: true): Start in fullscreen mode.
- **tiles** (boolean, default: true): Use graphical tiles instead of ASCII.

#### `panes` Array

Array of pane configuration objects. Panes are arranged in the order specified.

```json
{
  "panes": [
    {
      "type": "INVENTORY",
      "where": "RIGHT"
    },
    {
      "type": "LOG",
      "where": "BOTTOM",
      "rows": 6
    }
  ]
}
```

**Pane Types:**
- `MAIN` - Main game view (dungeon map)
- `INVENTORY` - Inventory display
- `WORN` - Equipment/worn items
- `ROLLS` - Combat rolls display
- `INFO` - Monster/item information
- `CHARACTER` - Character sheet
- `LOG` - Message log
- `MONSTERS` - Visible monsters list

**Pane Fields:**
- **type** (string, required): Which pane to display
- **where** (string, required): Placement - either `"RIGHT"` or `"BOTTOM"`
- **rows** (integer, optional): Fixed number of rows
- **cols** (integer, optional): Fixed number of columns
- **ratio** (float, optional): Proportional size along secondary axis (0.0-1.0)

### Example Configurations

#### Minimal Setup (Main view only)
```json
{
  "sdl": {
    "mainViewScale": 1,
    "fullscreen": true,
    "tiles": true
  },
  "panes": []
}
```

#### Classic Layout (Inventory + Log)
```json
{
  "sdl": {
    "mainViewScale": 1,
    "auxViewFontSize": 18,
    "fullscreen": true,
    "tiles": true
  },
  "panes": [
    {
      "type": "INVENTORY",
      "where": "RIGHT"
    },
    {
      "type": "LOG",
      "where": "BOTTOM",
      "rows": 6
    }
  ]
}
```

#### Full Information Layout
```json
{
  "sdl": {
    "mainViewScale": 2,
    "auxViewFontSize": 20,
    "margin": 8,
    "fullscreen": true,
    "tiles": true
  },
  "panes": [
    {
      "type": "INVENTORY",
      "where": "RIGHT"
    },
    {
      "type": "WORN",
      "where": "RIGHT"
    },
    {
      "type": "INFO",
      "where": "RIGHT",
      "rows": 8
    },
    {
      "type": "ROLLS",
      "where": "BOTTOM",
      "rows": 4
    },
    {
      "type": "LOG",
      "where": "BOTTOM"
    }
  ]
}
```

#### Large Tiles ASCII Mode (Windowed)
```json
{
  "sdl": {
    "mainViewScale": 3,
    "auxViewFontSize": 24,
    "margin": 12,
    "fullscreen": false,
    "tiles": false
  },
  "panes": [
    {
      "type": "INVENTORY",
      "where": "RIGHT"
    },
    {
      "type": "LOG",
      "where": "BOTTOM",
      "rows": 8
    }
  ]
}
```

### Notes

1. The main view automatically adjusts to fill remaining space after panes are placed.
2. If the main view would be smaller than 80x25 after pane placement, panes are automatically removed to maintain minimum size.
3. JSON syntax must be valid (use quotes for strings, no trailing commas).
4. Changes to the JSON file are applied on next game launch.
5. The file is saved with current settings when the game exits normally.
6. Use a JSON validator if you're hand-editing the file to ensure syntax is correct.

### Troubleshooting

**Invalid JSON Syntax**
If the JSON file has syntax errors, the game will log an error and use defaults. Check the log file for error messages. Common issues:
- Missing quotes around strings
- Trailing commas in arrays/objects
- Mismatched brackets `{}` or `[]`

**Settings Not Applied**
- Ensure JSON is valid
- Check file permissions (must be readable)
- Look in log file for "Loading SDL configuration from:" message

**Panes Not Appearing**
- Window may be too small (check log for "main view too small" warnings)
- Verify JSON syntax for panes array
- Ensure `type` and `where` values are correct and properly quoted
