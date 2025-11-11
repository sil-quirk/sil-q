# Custom Font Configuration for Story Messages

## Overview
Sil-More SDL3 now supports custom fonts for story messages and banners. This allows you to use proportional (non-monospace) fonts for narrative text, making story sequences and depth-change banners more visually appealing.

## Configuration

Add the following fields to your `sil_sdl.json` file (located in `%USERPROFILE%\sil-more` on Windows or `~/sil-more` on macOS/Linux):

```json
{
  "sdl": {
    "mainViewScale": 4,
    "auxViewFontSize": 16,
    "margin": 4,
    "fullscreen": true,
    "tiles": true,
    "storyFont": "lib/xtra/font/YourStoryFont.ttf",
    "bannerFont": "lib/xtra/font/YourBannerFont.ttf",
    "windowX": 100,
    "windowY": 100,
    "windowWidth": 1920,
    "windowHeight": 1080
  },
  "panes": [
    ...
  ]
}
```

## Font Fields

### `storyFont` (optional)
- **Purpose**: Font used for main story text in narrative sequences
- **Default**: `lib/xtra/font/InputMono-Bold.ttf` (if not specified or file not found)
- **Size**: 24px
- **Example**: `"storyFont": "lib/xtra/font/SpecialElite-Regular.ttf"`

### `bannerFont` (optional)
- **Purpose**: Font used for prominent banners (e.g., depth changes, important events)
- **Default**: `lib/xtra/font/InputMono-Bold.ttf` (if not specified or file not found)
- **Size**: 32px
- **Example**: `"bannerFont": "lib/xtra/font/Cinzel-Bold.ttf"`

## Font Recommendations

### For Story Text
- Use readable serif or sans-serif fonts
- Recommended: Book Antiqua, Garamond, Georgia, Palatino
- Should be clear at 24px size

### For Banners
- Use bold, impactful fonts
- Recommended: Cinzel, Trajan, Cormorant, IM Fell
- Should be dramatic and readable at 32px

## Installing Custom Fonts

1. Obtain TTF font files (ensure they're licensed for your use)
2. Place them in the `lib/xtra/font/` directory
3. Reference them in `sil_sdl.json` using relative paths
4. Restart the game

## Fallback Behavior

If a custom font cannot be loaded:
1. The game logs a warning message
2. Automatically falls back to `InputMono-Bold.ttf`
3. Continues running normally

This ensures the game never crashes due to missing fonts.

## Example Configurations

### Fantasy Theme
```json
"storyFont": "lib/xtra/font/Garamond.ttf",
"bannerFont": "lib/xtra/font/Cinzel-Bold.ttf"
```

### Classic Book Theme
```json
"storyFont": "lib/xtra/font/Palatino.ttf",
"bannerFont": "lib/xtra/font/SpecialElite-Regular.ttf"
```

### Modern Clean Theme
```json
"storyFont": "lib/xtra/font/Lato-Regular.ttf",
"bannerFont": "lib/xtra/font/Lato-Bold.ttf"
```

## Troubleshooting

### Fonts Not Appearing
1. Check that font paths are correct and relative to the game executable
2. Verify TTF files are valid and not corrupted
3. Look in `log.txt` for font loading messages
4. Ensure font file names match exactly (case-sensitive on some systems)

### Text Looks Wrong
- Some fonts may not render well at the default sizes
- Try different fonts or request adjustable font sizes as a feature

## Technical Details

- Font rendering uses SDL_ttf with anti-aliasing (blended mode)
- Proportional spacing is fully supported
- Text can overflow terminal cell boundaries for better appearance
- Color attributes from the angband color table are preserved
- Non-SDL builds fall back to standard monospace rendering

## Where Custom Fonts Are Used

Currently, custom fonts are used in:
- **Story sequences**: The `pause_with_text()` function (triggered during special events)
- **Depth change banners**: When entering new dungeon levels (if configured)

Regular game text (menus, combat log, etc.) continues to use monospace fonts for proper alignment.
