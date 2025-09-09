# AI Coding Agent Instructions for Sil-Morë

## Project Overview
Sil-Morë is a Tolkien-themed ASCII roguelike game forked from Sil, which itself descends from Angband. The game features stealth mechanics, Tolkien lore integration, and a complex quest system involving the Valar (Tulkas, Aule, Mandos).

## Architecture & Build System

### Core Structure
- **Primary Language**: C (C89/C90 compatible)
- **Main Header**: `src/angband.h` - Include this in all source files
- **Version**: 0.8.5 (see `defines.h` for version constants)
- **Platform Support**: Windows, macOS, Linux/Unix, DOS

### Build System
Multiple platform-specific Makefiles exist:
- `Makefile.std` - Unix/Linux systems
- `Makefile.cyg` - Cygwin on Windows  
- `Makefile.win` - Windows native
- `Makefile.osx` - macOS
- `Makefile.cocoa` - macOS with Cocoa interface

### Module Architecture
The codebase uses modular platform-specific main files:
- `main-gcu.c` - Curses interface (Unix/Linux)
- `main-win.c` - Windows interface
- `main-cocoa.m` - macOS Cocoa interface
- `main-x11.c` - X11 interface

## Key Files & Responsibilities

### Core Engine Files
- `angband.h` - Central header with all includes
- `defines.h` - Game constants, version info, compile-time flags
- `types.h` - Data structures (player_type, monster_type, etc.)
- `externs.h` - External variable declarations
- `config.h` - Compilation configuration

### Game Logic
- `dungeon.c` - Main game loop, turn processing, player actions
- `generate.c` - Dungeon/vault generation, monster placement
- `cave.c` - Map/terrain management
- `xtra1.c`, `xtra2.c` - Utility functions, quest logic
- `monster1.c`, `monster2.c` - Monster behavior and AI
- `cmd1.c`-`cmd6.c` - Player command processing

### Data Management
- `save.c`, `load.c` - Save file serialization/deserialization
- `files.c` - File I/O operations
- `init1.c`, `init2.c` - Game data initialization
- `tables.c` - Static game data tables

### Quest System
- Quest state stored in `player_type` structure
- Logic implemented in `xtra2.c` (interactions) and `generate.c` (spawning)
- Metarun system in `metarun.c` for cross-game persistence

### Data Files (lib/edit/)
Text-based configuration files parsed at startup:
- `monster.txt` - Monster definitions (N:ID:Name format)
- `vault.txt` - Dungeon vault layouts and templates  
- `artefact.txt` - Artifact definitions
- `object.txt` - Item/object definitions
- `ability.txt` - Player abilities and skills

## Coding Conventions

### Style Guidelines
1. **Function Naming**: Use snake_case (e.g., `place_monster_one`)
2. **Constants**: Use UPPER_CASE with underscores (e.g., `R_IDX_MANDOS`)
3. **Variables**: Use snake_case (e.g., `monster_type *m_ptr`)
4. **Indentation**: Use tabs, not spaces
5. **Braces**: Opening brace on same line for functions and control structures

### Memory Management
- Use `MAKE()` macro for array allocation
- Use `KILL()` macro for freeing memory
- Always check for NULL pointers
- Initialize variables to prevent undefined behavior

### Error Handling
- Use `log_trace()` for debugging output
- Use `msg_print()` for player messages
- Check bounds before array access
- Validate function parameters

### Data Structure Access
- Player data: `p_ptr->field_name`
- Monster data: `m_ptr->field_name` 
- Monster race: `r_ptr->field_name`
- Cave/terrain: `cave_feat[y][x]`, `cave_info[y][x]`

## Quest System Implementation

### Quest States
Use enum constants defined in `defines.h`:
```c
#define QUEST_NOT_STARTED 0
#define QUEST_GIVER_PRESENT 1  
#define QUEST_ACTIVE 2
#define QUEST_SUCCESS 3
```

### Adding New Quests
1. **Define constants** in `defines.h`:
   ```c
   #define R_IDX_NEW_QUEST_GIVER 21
   #define NEW_QUEST_NOT_STARTED 0
   // ... etc
   ```

2. **Add player fields** in `types.h`:
   ```c
   typedef struct player_type {
       // ... existing fields
       byte new_quest;
       s16b new_quest_level;
       // ... etc
   } player_type;
   ```

3. **Create monster entry** in `lib/edit/monster.txt`:
   ```
   N:21:Quest Giver Name
   W:5:1
   G:V:color
   F:NEVER_MOVE | PEACEFUL | UNIQUE | NEVER_BLOW
   D:Description text here.
   ```

4. **Add vault symbol** in `generate.c` monster placement:
   ```c
   case 'Q': // New symbol
   {
       place_monster_one(y, x, R_IDX_NEW_QUEST_GIVER, true, true, NULL);
       break;
   }
   ```

5. **Implement interaction logic** in `xtra2.c`:
   ```c
   void new_quest_interaction(void)
   {
       // Quest logic here
   }
   ```

6. **Add save/load support** in `save.c` and `load.c`:
   ```c
   // In save.c
   wr_byte(p_ptr->new_quest);
   wr_s16b(p_ptr->new_quest_level);
   
   // In load.c  
   p_ptr->new_quest = rd_byte();
   p_ptr->new_quest_level = rd_s16b();
   ```

7. **Initialize in birth.c**:
   ```c
   p_ptr->new_quest = NEW_QUEST_NOT_STARTED;
   ```

### Vault Creation
Create entries in `lib/edit/vault.txt`:
```
N:500:Vault Name
X:6:5:7:9:11  # Type:Rarity:MinLevel:MaxLevel:MaxDepth
F:QUEST | LIGHT  # Flags
D:######   # Layout using symbols
D:#....#   # . = floor, # = wall
D:#.Q..#   # Q = quest giver symbol  
D:#....#
D:######
```

## Metarun System

### Purpose
The metarun system preserves certain data across multiple playthroughs, including quest completion status.

### Implementation
- Core logic in `metarun.c`
- Check completion: `metarun_is_quest_completed(METARUN_QUEST_NAME)`
- Mark completion: `metarun_complete_quest(METARUN_QUEST_NAME)`
- Add new constants to `defines.h` for new metarun quests

## Common Patterns

### Monster Spawning
```c
// Check if monster already exists
for (i = 1; i < mon_max; i++) {
    monster_type *m_ptr = &mon_list[i];
    if (m_ptr->r_idx == R_IDX_TARGET) {
        already_exists = true;
        break;
    }
}

// Spawn monster at location
if (cave_floor_bold(y, x) && cave_m_idx[y][x] == 0) {
    place_monster_one(y, x, R_IDX_TARGET, true, true, NULL);
}
```

### Player Adjacency Checks
```c
// Check if player is adjacent to monster
for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++) {
    for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++) {
        if (cave_m_idx[y][x] > 0) {
            monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];
            if (m_ptr->r_idx == R_IDX_TARGET) {
                // Player is adjacent
            }
        }
    }
}
```

### Area Clearing Checks
```c
// Count monsters in vault area
int monster_count = 0;
for (int y = vault_y - radius; y <= vault_y + radius; y++) {
    for (int x = vault_x - radius; x <= vault_x + radius; x++) {
        if (in_bounds(y, x) && cave_m_idx[y][x] > 0) {
            monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];
            // Don't count quest givers or peaceful monsters
            if (m_ptr->r_idx != R_IDX_QUEST_GIVER && 
                !(r_info[m_ptr->r_idx].flags1 & RF1_PEACEFUL)) {
                monster_count++;
            }
        }
    }
}
```

## Debugging & Logging

### Debug Output
Use `log_trace()` for development debugging:
```c
log_trace("Quest state changed from %d to %d", old_state, new_state);
log_trace("Monster spawned at (%d,%d): r_idx=%d", y, x, r_idx);
```

### Player Messages
Use `msg_print()` for player-visible messages:
```c
msg_print("The quest giver appears before you.");
msg_format("You have %d monsters remaining to defeat.", count);
```

## Testing & Validation

### Compilation Testing
Test on multiple platforms using appropriate Makefiles:
```bash
# Unix/Linux
make -f Makefile.std

# Windows (Cygwin)
make -f Makefile.cyg

# macOS  
make -f Makefile.osx
```

### Save/Load Testing
- Always test save/load after adding new player fields
- Verify version compatibility in save files
- Test both new games and loaded games

### Quest Testing Scenarios
1. Quest giver appearance conditions
2. Quest progression triggers  
3. Quest completion detection
4. Metarun persistence across games
5. Edge cases (quest giver death, level changes)

## Common Pitfalls

### Monster Index Management
- Monster indices can change when monsters die
- Always validate `cave_m_idx[y][x] > 0` before accessing `mon_list`
- Use `m_ptr->r_idx` to check monster race, not the array index

### Quest State Persistence
- Add new quest fields to both `save.c` and `load.c`
- Initialize quest states in `birth.c`
- Consider metarun implications for cross-game persistence

### Vault Symbol Conflicts
- Check existing vault symbols in `vault.txt` before adding new ones
- Update both vault templates and monster placement code in `generate.c`
- Test vault generation doesn't break existing content

### Memory Management
- Use appropriate MAKE/KILL macros
- Don't assume pointers remain valid across function calls
- Initialize arrays and structures to prevent garbage data

## Integration Guidelines

When modifying existing systems:

1. **Preserve Compatibility**: Maintain save file compatibility with older versions
2. **Follow Patterns**: Use existing code patterns for similar functionality  
3. **Test Thoroughly**: Verify changes work across all supported platforms
4. **Document Changes**: Update relevant comments and documentation
5. **Consider Performance**: Avoid expensive operations in frequently-called functions

This codebase has been stable for many years - prefer incremental changes over large refactoring when possible.
