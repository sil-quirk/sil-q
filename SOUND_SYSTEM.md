# Sound System Configuration Guide

## Overview

The sound system has been redesigned to use folder-based configuration stored in `sil_sdl.json`. Each sound event can be mapped to a folder, and the game will randomly select one audio file from that folder when the event triggers.

## Configuration Location

Sound settings are stored in a separate `sound.json` file in your Sil-More user folder:
- **Windows**: `%USERPROFILE%\sil-more\sound.json`
- **macOS/Linux**: `~/sil-more/sound.json`

## Sound Settings Structure

The `sound.json` file contains all sound configuration:

```json
{
  "enabled": true,
  "sampleRate": 22050,
  "channels": 2,
  "format": "s16",
  "events": {
    "hit": "sound/SFX/Attacks/Sword_Attacks_Hits_and_Blocks",
    "miss": "sound/Minifantasy_Weapons_SFX/Swing_Attacks",
    "shoot": "sound/Minifantasy_Weapons_SFX/Ranged_Attacks"
  }
}
```

### Sound Settings Fields

- **enabled** (boolean): Enable or disable game sounds (default: false)
- **sampleRate** (number): Audio sample rate in Hz (default: 22050)
- **channels** (number): Audio channels - 1 for mono, 2 for stereo (default: 2)
- **format** (string): Audio format - "s8", "u8", "s16", "s32", or "f32" (default: "s16")
- **events** (object): Maps sound event names to folder paths

## Sound Events

Each event name corresponds to a game action. Available events include:

- **hit**: Melee attack hits
- **miss**: Melee attack misses
- **shoot**: Ranged attacks
- **weapon_slash_light**: Light slashing weapons
- **weapon_slash_medium**: Medium slashing weapons
- **weapon_slash_heavy**: Heavy slashing weapons
- **weapon_thrust**: Thrusting/piercing attacks
- **weapon_blunt**: Blunt weapon attacks
- **weapon_unarmed**: Unarmed combat
- **armor**: Armor deflection sounds
- **dig**: Mining/digging
- **opendoor**: Opening doors
- **shutdoor**: Closing doors
- **walk**: Footsteps
- **eat**: Consuming food
- **quaff**: Drinking potions
- **drop**: Dropping items
- **kill**: Killing monsters
- **level**: Level up
- **death**: Character death

For a complete list, see `angband_sound_name[]` in `src/variable.c`.

## Folder Paths

Folder paths can be:

1. **Relative to lib/xtra**: `"sound/SFX/Footsteps/Stone"`
2. **Absolute paths**: `"C:/sounds/custom/footsteps"` (Windows) or `"/home/user/sounds/footsteps"` (Unix)

The game will scan the folder and randomly select from all `.wav` files found.

## Example Configuration

See `sound.json` in the repository root for a complete example that maps all common sound events to the included sound packs.

## Legacy sound.cfg

The old `lib/xtra/sound/sound.cfg` file is **no longer used**. All sound configuration is now in `sil_sdl.json`.

## Tips

1. **Organize by category**: Group related sounds in folders (e.g., all sword hits in one folder)
2. **Mix and match**: Different weapons can use different sound sets for variety
3. **Test incrementally**: Start with a few events, then add more as needed
4. **Check the log**: The game logs which sound files are loaded at startup (`log.txt`)

## Troubleshooting

- **No sounds playing**: Check that `"enabled": true` in `sound.json`
- **Can't find sound files**: Check paths are correct and files exist
- **Wrong sounds playing**: Verify folder contains only the sounds you want for that event
- **Audio quality issues**: Try adjusting `sampleRate` and `format` in `sound.json`
