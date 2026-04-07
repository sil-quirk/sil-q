# SDL Resolution Configuration Reference

Complete reference for all 41 supported resolutions with automatically configured layouts.

## Design Priority

**MAXIMUM SCALE** (up to 3) is prioritized for best graphics quality, then auxiliary panes are added if space permits.

## Configuration Summary

| # | Resolution | Name | Scale | Font | Right | Bottom | Layout |
|---|------------|------|-------|------|-------|--------|--------|
| 1 | 800×600 | SVGA | 1 | 9 | - | 4 | Bottom only |
| 2 | 1024×768 | XGA | 1 | 9 | 40 | 4 | Full |
| 3 | 1152×864 | | 1 | 9 | 50 | 4 | Full |
| 4 | 1280×720 | HD 720p | 1 | 9 | 50 | 4 | Full |
| 5 | 1280×768 | | **2** | 16 | - | - | **Main only** |
| 6 | 1280×800 | WXGA | **2** | 16 | - | 2 | Bottom only |
| 7 | 1280×1024 | SXGA | **2** | 16 | - | 4 | Bottom only |
| 8 | 1360×768 | | **2** | 16 | - | - | **Main only** |
| 9 | 1366×768 | HD | **2** | 16 | - | - | **Main only** |
| 10 | 1400×1050 | | **2** | 16 | - | 4 | Bottom only |
| 11 | 1440×900 | | **2** | 16 | - | 4 | Bottom only |
| 12 | 1536×864 | | **2** | 16 | - | 4 | Bottom only |
| 13 | 1600×900 | HD+ | **2** | 16 | - | 4 | Bottom only |
| 14 | 1600×1200 | UXGA | **2** | 16 | - | 4 | Bottom only |
| 15 | 1680×1050 | | **2** | 16 | 40 | 4 | Full |
| 16 | 1920×1080 | Full HD | **2** | 16 | 50 | 4 | Full |
| 17 | 1920×1200 | WUXGA | **3** | 18 | - | 2 | Bottom only |
| 18 | 2048×1152 | | **3** | 18 | - | - | **Main only** |
| 19 | 2160×1440 | | **3** | 18 | - | 4 | Bottom only |
| 20 | 2560×1080 | Ultrawide | **2** | 16 | 50 | 4 | Full |
| 21 | 2560×1440 | QHD | **3** | 18 | 50 | 4 | Full |
| 22 | 2560×1600 | MacBook 13" | **3** | 18 | 50 | 4 | Full |
| 23 | 2736×1824 | Surface Book | **3** | 18 | 50 | 4 | Full |
| 24 | 2880×1620 | | **3** | 18 | 50 | 4 | Full |
| 25 | 2880×1800 | MacBook 15" | **3** | 18 | 50 | 4 | Full |
| 26 | 3000×2000 | Surface Laptop | **3** | 18 | 50 | 4 | Full |
| 27 | 3200×1800 | | **3** | 18 | 50 | 4 | Full |
| 28 | 3240×2160 | | **3** | 18 | 50 | 4 | Full |
| 29 | 3440×1440 | Ultrawide QHD | **3** | 18 | 50 | 4 | Full |
| 30 | 3840×1080 | Super Ultrawide | **2** | 16 | 50 | 4 | Full |
| 31 | 3840×1200 | | **3** | 18 | 50 | 4 | Full |
| 32 | 3840×1440 | | **3** | 18 | 50 | 4 | Full |
| 33 | 3840×1600 | | **3** | 18 | 50 | 4 | Full |
| 34 | 3840×2160 | 4K UHD | **3** | 18 | 50 | 4 | Full |
| 35 | 4096×2160 | DCI 4K | **3** | 18 | 50 | 4 | Full |
| 36 | 4480×1440 | | **3** | 18 | 50 | 4 | Full |
| 37 | 5120×1440 | Super Ultrawide | **3** | 18 | 50 | 4 | Full |
| 38 | 5120×2160 | 5K Ultrawide | **3** | 18 | 50 | 4 | Full |
| 39 | 5120×2880 | 5K | **3** | 18 | 50 | 4 | Full |
| 40 | 6016×3384 | 6K | **3** | 18 | 50 | 4 | Full |
| 41 | 7680×4320 | 8K UHD | **3** | 18 | 50 | 4 | Full |

## Scale Distribution

- **Scale 1 (4 resolutions)**: 800×600 through 1280×720 - Legacy/low-DPI displays
- **Scale 2 (14 resolutions)**: 1280×768 through 3840×1080 - Standard HD displays  
- **Scale 3 (23 resolutions)**: 1920×1200 through 7680×4320 - High-DPI/Retina displays

## Layout Types

- **Main only (3)**: Exact fits with no room for panes
  - 1280×768, 1360×768, 1366×768, 2048×1152
- **Bottom only (10)**: Vertical space for log/rolls but no right sidebar
  - 800×600, 1280×800, 1280×1024, 1400×1050, 1440×900, 1536×864, 1600×900, 1600×1200, 1920×1200, 2160×1440
- **Full (28)**: Both right sidebar and bottom panes
  - All others

## Minimum Requirements

- **For any display**: 640×384 pixels (scale 1)
- **For scale 2**: 1280×768 pixels
- **For scale 3**: 1920×1152 pixels

## Layout Components

### Main Terminal
- Minimum: 40×24 tiles
- Expands to use all remaining space after panes allocated

### Right Pane (when ≥40 columns available)
- **Inventory**: 22 rows
- **Worn Equipment**: 17 rows  
- **Info**: Auto-fills remaining height
- **Width**: 40 columns (tight) or 50 columns (comfortable)

### Bottom Pane (when ≥1 row available, max 4)
- **Rolls**: 50% of height (rounded)
- **Log**: Remaining 50%

## Special Cases

### Exact Fits (No Panes)
These resolutions fit the minimum terminal exactly with no extra space:
- **1280×768** @ scale 2
- **1360×768** @ scale 2  
- **1366×768** @ scale 2 (common laptops)
- **2048×1152** @ scale 3

### Limited Bottom Pane
- **1280×800** @ scale 2: Only 2 rows (32px remaining)
- **1920×1200** @ scale 3: Only 2 rows (48px remaining)

### First Scale Transitions
- **1680×1050**: First scale-2 resolution with right pane
- **1920×1080**: Full HD, most popular resolution, full panes @ scale 2
- **1920×1200**: First scale-3 resolution

### Ultrawide Displays
- **2560×1080**: Full panes @ scale 2
- **3440×1440**: Full panes @ scale 3
- **3840×1080**: Super ultrawide @ scale 2
- **5120×1440**: Extreme ultrawide @ scale 3

## Testing Priority

1. ✅ **1920×1080** - Most common
2. ✅ **2560×1440** - Popular gaming/work
3. ✅ **1366×768** - Common laptop (no panes)
4. ✅ **3840×2160** - 4K standard
5. ✅ **1280×800** - Steam Deck
6. ⬜ **1680×1050** - First scale-2 with right pane
7. ⬜ **3440×1440** - Ultrawide QHD
8. ⬜ **2048×1152** - Exact fit test

## Configuration File

When `sil_sdl.json` (stored in your Sil-More user folder—`%USERPROFILE%\sil-more\sil_sdl.json` on Windows or `~/sil-more/sil_sdl.json` on macOS/Linux) doesn't exist, these defaults are applied automatically based on detected screen resolution. Users can override any setting by:
1. Creating or editing that `sil_sdl.json` file
2. Using command-line arguments (`--scale`, `--font-size`, etc.)
