# Sound System Migration Guide

## What Changed?

The sound system has been completely restructured to use folder-based configuration in `sil_sdl.json` instead of the old `sound.cfg` file.

## Key Improvements

1. **Separate sound configuration**: Sound settings are now in their own `sound.json` file
2. **Folder-based selection**: Point an event to a folder, and the game randomly picks from all audio files in that folder
3. **No manual file lists**: Just drop files in a folder - no need to list them individually
4. **Easier organization**: Group related sounds naturally by folder
5. **More variety**: Use entire sound packs without configuration overhead

## Migration Steps

### 1. Create sound.json

Create a new `sound.json` file in your Sil-More user folder (`%USERPROFILE%\sil-more\` on Windows or `~/sil-more/` on macOS/Linux):

```json
{
  "enabled": true,
  "sampleRate": 22050,
  "channels": 2,
  "format": "s16",
  "events": {
  }
}
```

### 2. Map Events to Folders

For each sound event you want to configure, add an entry under `"events"`:

```json
"events": {
  "hit": "sound/SFX/Attacks/Sword_Attacks_Hits_and_Blocks",
  "miss": "sound/Minifantasy_Weapons_SFX/Swing_Attacks",
  "walk": "sound/SFX/Footsteps/Stone"
}
```

### 3. Use the Example Configuration

A complete example configuration is provided in `sound.json` at the repository root. You can copy this file to your user folder to get started quickly.

## Old vs New

### Before (sound.cfg):
```ini
[Sound]
hit = Hit_Metal_on_leather Hit_Metal_on_flash Hit_Wood_on_flesh
miss = 
walk = Stone_Walk_1 Stone_Walk_2 Stone_Walk_3
```

Problems:
- Had to list every file individually
- Multiple config file formats
- Hard to manage large sound sets
- Adding new sounds required editing config

### After (sound.json):
```json
{
  "enabled": true,
  "events": {
    "hit": "sound/SFX/Attacks/Sword_Attacks_Hits_and_Blocks",
    "walk": "sound/SFX/Footsteps/Stone"
  }
}
```

Benefits:
- Just point to a folder
- Game finds all audio files automatically
- Easy to add/remove sounds - just modify the folder contents
- One config file for everything

## Available Sound Events

See `SOUND_SYSTEM.md` for the complete list of available sound events.

Common events:
- `hit`, `miss` - Melee combat
- `shoot` - Ranged attacks
- `weapon_slash_light`, `weapon_slash_medium`, `weapon_slash_heavy` - Different weapon types
- `weapon_thrust`, `weapon_blunt`, `weapon_unarmed` - Attack types
- `armor` - Armor deflection
- `walk` - Footsteps
- `eat`, `quaff` - Consumption
- `opendoor`, `shutdoor` - Doors
- `dig` - Mining
- `kill`, `death` - Combat outcomes
- `level` - Leveling up

## Supported Audio Formats

- `.wav` - Wave audio files
- `.ogg` - Ogg Vorbis files

## Troubleshooting

**Q: Sounds aren't playing**
- Check that `"enabled": true` in `sound.json`
- Verify folder paths are correct
- Look in `log.txt` for error messages

**Q: Wrong sounds are playing**
- Check the folder contains only the sounds you want for that event
- Remember the game picks randomly from ALL files in the folder

**Q: How do I disable specific sounds?**
- Just don't include that event in `"events"`
- Or set the path to an empty string: `"event_name": ""`

**Q: Can I use custom sound packs?**
- Yes! Use absolute paths: `"hit": "C:/MySounds/combat"`
- Or add your sounds to `lib/xtra/sound/` and use relative paths

## Need Help?

- Maximum 64 sound files per event (increased from 16)
- File paths can be up to 256 characters
- Folder scanning uses native OS APIs (FindFirstFile on Windows, opendir on Unix)
- Random selection uses the game's RNG for fairness
- Audio mixing supports concurrent sounds (attacks can overlap)

## Need Help?

- See `SOUND_SYSTEM.md` for detailed documentation
- Check `sound.json` for a working example
- Look in `log.txt` for diagnostic messages about sound loading
