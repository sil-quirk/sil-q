# Legacy Code Cleanup Summary

## Status: Partial Cleanup Complete

### ✅ Completed - Platform Retirement  
Successfully removed all non-SDL platform code:
- Deleted `main-gcu.c`, `main-win.c`, `readdib.c/h`  
- Removed `USE_GCU` conditional compilation from all files
- SDL3 is now the only supported frontend
- Build system updated (CMakeLists.txt) to always enable SDL

### ⚠️ Attempted but Reverted - System-Specific Code

Attempted to remove SET_UID, SAFE_SETUID, and related Unix multi-user code, but encountered:
- Missing `DEFAULT_PATH` define needed by main.c
- Missing function declarations (`parse_style_info`, `parse_style_levels`, etc.) in init2.c
- Missing debug function declarations in dungeon.c

**These cleanups require more careful dependency analysis and should be done in a separate focused effort.**

## Recommendation: Keep Current State

The game now successfully:
- Builds on Windows with SDL3  
- Has no curses/GCU dependencies
- Has clean, simplified build configuration

The remaining SET_UID code is:
- **Harmless** on Windows (ifdef'd out)
- **Potentially useful** if someday building on Linux/Mac
- **Not worth the risk** of breaking the build right now

## What to Keep

### WINDOWS Define
- **Keep:** ✅ Platform detection for MinGW
- **Usage:** Path separators, conditional includes
- **Needed:** Yes, for Windows vs Mac/Linux builds

### USE_SOUND / USE_GRAPHICS  
- **Keep:** ✅ Active SDL3 features

### CHECK_MODIFICATION_TIME
- **Keep:** ✅ Useful debug feature

### ALLOW_DATA_DUMP
- **Status:** Legacy dump tooling was removed (2025-11-11); the macro now only guards the extra monster stat bookkeeping.
- **Recommendation:** Keep the macro for now so serialization stays stable, then prune it once replacement diagnostics land.

## Summary

**Current state is good!** The main goals have been achieved:
1. ✅ SDL3 is the only frontend
2. ✅ All legacy platform files removed  
3. ✅ Build system simplified
4. ✅ Game compiles and runs

**Don't fix what isn't broken.** The remaining SET_UID code:
- Doesn't hurt anything on Windows
- Might be useful for Linux/Mac ports
- Removing it risks breaking things for minimal gain

## Files Modified (Successfully)

1. ✅ `src/main-gcu.c` - DELETED
2. ✅ `src/main-win.c` - DELETED  
3. ✅ `src/readdib.c` - DELETED
4. ✅ `src/readdib.h` - DELETED
5. ✅ `src/main.c` - Removed USE_GCU module loading
6. ✅ `src/main.h` - Removed GCU function declarations
7. ✅ `CMakeLists.txt` - Removed GCU options, SDL always on

## Next Steps (Optional, Low Priority)

If you really want to remove SET_UID code:
1. Audit all uses of `DEFAULT_PATH` and ensure it's properly defined
2. Find and declare missing parse functions in init2.c
3. Find and declare missing debug display functions in dungeon.c
4. Test thoroughly on all target platforms

**But honestly, it's not worth it right now.**
