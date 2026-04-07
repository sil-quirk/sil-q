# SDL File I/O Migration - Technical Documentation

## Objective
Migrate all file operations from proprietary/standard C functions (FILE*, fd operations) to SDL3's SDL_IOStream API for platform-independent file handling.

## Completed (2025-11-10)

### 1. Created SDL IOStream Wrapper Functions (util.c)
Added comprehensive SDL-based file I/O API:
- `sdl_fopen()` - Opens files using SDL_IOFromFile
- `sdl_fclose()` - Closes SDL_IOStream
- `sdl_fopen_temp()` - Creates temporary files
- `sdl_fgets()` - Reads lines with tab expansion and CR/LF handling
- `sdl_fputs()` - Writes lines using SDL_IOprintf
- `sdl_read()` - Binary read with error checking
- `sdl_write()` - Binary write with error checking
- `sdl_seek()` - File positioning
- `sdl_tell()` - Get current position
- `sdl_size()` - Get stream size

### 2. Updated Headers
- Modified angband.h to include SDL3/SDL.h before externs.h
- Updated externs.h:
  - Added declarations for new SDL functions under `#ifdef USE_SDL`
  - Marked old my_fopen/my_fclose/fd_* functions as DEPRECATED
  - Updated text_out_file and highscore_fd to conditionally use SDL_IOStream*

### 3. Updated Global Variables (variable.c)
- `text_out_file` - Now SDL_IOStream* when USE_SDL
- `highscore_fd` - Now SDL_IOStream* when USE_SDL

## Remaining Work

### Files Requiring Migration

#### High Priority - Core Gameplay
1. **save.c** - Savefile writing
   - Replace static `FILE* fff` with `SDL_IOStream* fff`
   - Replace `putc()` with `SDL_WriteIO()`
   - Test savefile compatibility

2. **load.c** - Savefile loading
   - Replace static `FILE* fff` with `SDL_IOStream* fff`
   - Replace `fgetc()` with `SDL_ReadIO()`

3. **init1.c** - Text template parsing
   - Replace `my_fopen()` → `sdl_fopen()`
   - Replace `my_fclose()` → `sdl_fclose()`
   - Replace `my_fgets()` → `sdl_fgets()`

4. **init2.c** - Binary/text template parsing
   - Migrate fd operations to SDL_IOStream
   - Update binary parsing logic

#### Medium Priority - Ancillary Systems
5. **metarun.c** - Meta score file
   - Replace fd operations with SDL_IOStream
   - Update `fd_seek()` → `sdl_seek()`

6. **wizard1.c** - Spoiler generation
   - Replace `fprintf()` with `SDL_IOprintf()`
   - Replace `ferror()` checks

7. **squelch.c** - Auto-inscription
   - Simple FILE* to SDL_IOStream conversion

8. **sdl-config.c** - JSON config files
   - Replace `fopen/fclose/fread` with SDL functions

9. **main-sdl.c** - Config file checking
   - Update file existence checks

#### Low Priority - Optional
10. **util.c** - Log file creation (line 6010)
11. **log/log.c** - Logging system (may keep as-is)
12. **main-win.c** - Windows INI files (platform-specific)

### Functions to Delete (After Migration)
From util.c:
- `my_fopen()`, `my_fclose()`, `my_fgets()`, `my_fputs()`
- `my_fopen_temp()`
- `fd_open()`, `fd_close()`, `fd_read()`, `fd_write()`, `fd_seek()`
- `fd_make()`, `fd_lock()`

Keep (use standard C):
- `fd_kill()` - uses `remove()`
- `fd_move()` - uses `rename()`
- `fd_copy()` - needs implementation

## Migration Pattern Examples

### Simple FILE* Migration
```c
// OLD
FILE* fff = my_fopen(path, "r");
if (fff) {
    my_fgets(fff, buf, sizeof(buf));
    my_fclose(fff);
}

// NEW
SDL_IOStream* stream = sdl_fopen(path, "r");
if (stream) {
    sdl_fgets(stream, buf, sizeof(buf));
    sdl_fclose(stream);
}
```

### Binary I/O Migration
```c
// OLD (file descriptor)
int fd = fd_open(path, O_RDONLY);
fd_read(fd, buffer, size);
fd_close(fd);

// NEW (SDL_IOStream)
SDL_IOStream* stream = sdl_fopen(path, "rb");
sdl_read(stream, buffer, size);
sdl_fclose(stream);
```

### fprintf Migration
```c
// OLD
fprintf(fff, "Format: %d %s\n", value, string);

// NEW
SDL_IOprintf(stream, "Format: %d %s\n", value, string);
```

### putc/getc Migration (save.c/load.c)
```c
// OLD
byte c;
putc(c, fff);

// NEW
byte c;
SDL_WriteIO(stream, &c, 1);

// OLD
int c = fgetc(fff);
if (c == EOF) error();

// NEW
byte c;
if (SDL_ReadIO(stream, &c, 1) != 1) error();
```

## Testing Strategy

1. **Incremental compilation** - Test build after each file migration
2. **Savefile compatibility** - Ensure old saves load correctly
3. **Round-trip testing** - Save and load characters
4. **Template loading** - Verify game data loads
5. **Config persistence** - Check settings save/load

## Build Command
```
build-cmake.bat
```

## Known Issues / Considerations

1. **Binary compatibility** - SDL_IOStream may handle binary differently
2. **Error handling** - SDL uses different error reporting (SDL_GetError())
3. **File locking** - fd_lock() has no SDL equivalent, may need platform-specific code
4. **Performance** - SDL adds abstraction layer, monitor for issues

## Progress Tracking

- [x] Create SDL wrapper functions
- [x] Update headers and global variables
- [ ] Migrate init1.c
- [ ] Migrate init2.c
- [ ] Migrate save.c
- [ ] Migrate load.c
- [ ] Migrate metarun.c
- [ ] Migrate wizard1.c
- [ ] Migrate squelch.c
- [ ] Migrate sdl-config.c
- [ ] Migrate main-sdl.c
- [ ] Delete deprecated functions
- [ ] Full regression testing

Last updated: 2025-11-10
