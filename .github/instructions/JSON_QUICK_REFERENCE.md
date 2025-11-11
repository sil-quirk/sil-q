# JSON Configuration Quick Reference

## File Location
- **File**: `sil_sdl.json`
- **Location**: Sil-More user folder (`%USERPROFILE%\sil-more` on Windows, `~/sil-more` on macOS/Linux)

## Minimal Configuration
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

## Full Configuration Example
```json
{
  "sdl": {
    "mainViewScale": 1,
    "auxViewFontSize": 18,
    "margin": 4,
    "fullscreen": true,
    "tiles": true
  },
  "panes": [
    {"type": "INVENTORY", "where": "RIGHT"},
    {"type": "WORN", "where": "RIGHT"},
    {"type": "INFO", "where": "RIGHT", "rows": 8},
    {"type": "ROLLS", "where": "BOTTOM", "rows": 4},
    {"type": "LOG", "where": "BOTTOM"}
  ]
}
```

## SDL Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `mainViewScale` | integer | 1 | Tile size multiplier (1-4) |
| `auxViewFontSize` | integer | 18 | Font size for side panels |
| `margin` | integer | 4 | Margin around panes (pixels) |
| `fullscreen` | boolean | true | Fullscreen vs windowed |
| `tiles` | boolean | true | Tiles vs ASCII |

## Pane Types

- `MAIN` - Main dungeon view
- `INVENTORY` - Inventory list
- `WORN` - Equipped items
- `ROLLS` - Combat rolls
- `INFO` - Monster/item info
- `CHARACTER` - Character sheet
- `LOG` - Message log
- `MONSTERS` - Visible monsters

## Pane Placement

- `RIGHT` - Right side of screen
- `BOTTOM` - Bottom of screen

## Pane Options

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `type` | string | ✅ | Pane type (see above) |
| `where` | string | ✅ | Placement (RIGHT/BOTTOM) |
| `rows` | integer | ❌ | Fixed row count |
| `cols` | integer | ❌ | Fixed column count |
| `ratio` | float | ❌ | Size ratio (0.0-1.0) |

## Command-Line Overrides

```bash
--scale 2              # mainViewScale = 2
--font-size 20         # auxViewFontSize = 20
--margin 8             # margin = 8
--fullscreen           # fullscreen = true
--windowed             # fullscreen = false
--tiles                # tiles = true
--ascii                # tiles = false
```

## Common Configurations

### Minimal (Main Only)
```json
{"sdl": {"tiles": true}, "panes": []}
```

### Classic (Inventory + Log)
```json
{
  "sdl": {"tiles": true},
  "panes": [
    {"type": "INVENTORY", "where": "RIGHT"},
    {"type": "LOG", "where": "BOTTOM", "rows": 6}
  ]
}
```

### Large Tiles
```json
{
  "sdl": {"mainViewScale": 2, "auxViewFontSize": 20, "tiles": true},
  "panes": [
    {"type": "INVENTORY", "where": "RIGHT"},
    {"type": "LOG", "where": "BOTTOM"}
  ]
}
```

### ASCII Mode
```json
{
  "sdl": {"tiles": false},
  "panes": [
    {"type": "INVENTORY", "where": "RIGHT"},
    {"type": "LOG", "where": "BOTTOM"}
  ]
}
```

## JSON Syntax Rules

✅ **DO**:
- Use double quotes for strings
- Use lowercase `true`/`false` for booleans
- Use numbers without quotes
- No trailing commas

❌ **DON'T**:
- Single quotes `'string'`
- Capitalized booleans `True`/`False`
- Trailing commas `{"a": 1,}`
- Comments (not in standard JSON)

## Validation

Online validators:
- https://jsonlint.com/
- https://jsonformatter.org/

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Settings not loading | Check JSON syntax with validator |
| File not found | Game creates it on first run |
| Parse error | See log file for error location |
| Panes missing | Window might be too small |

## Defaults

If file doesn't exist or has errors, defaults are:
- Scale: 1
- Font: 18
- Margin: 4
- Fullscreen: true
- Tiles: true
- Panes: Inventory, Worn, Info, Rolls, Log

## More Info

- User Guide: `SDL_CONFIG_JSON.md`
- Implementation: `SDL_CONFIG_JSON_IMPLEMENTATION.md`
- Migration: `MIGRATION_INI_TO_JSON.md`
