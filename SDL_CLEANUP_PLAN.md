# SDL-Only Code Cleanup Plan

## Phase 1: Fix Highscore Bug (CRITICAL)
- [ ] Fix highscore_add() - remove nested #ifdef, add SDL_FlushIO()
- [ ] Fix open_scores_file_versioned() - remove #ifdef blocks
- [ ] Test: Verify multiple scores show after quit

## Phase 2: Remove All Conditional Compilation
### files.c
- [ ] Remove all `#ifdef USE_SDL` / `#else` / `#endif` blocks
- [ ] Keep only SDL3 code paths
- [ ] Remove SCORE_FILE_TYPE/SCORE_FILE_CLOSE macros (use SDL directly)
- [ ] Remove CHAR_FILE_PRINTF macro (use SDL_IOprintf directly)

### util.c  
- [ ] Remove all `#ifdef USE_SDL` blocks
- [ ] Delete old my_fopen, my_fclose, my_fgets, my_fputs functions
- [ ] Delete all fd_* functions
- [ ] Keep only sdl_* functions

### wizard1.c
- [ ] Remove all SPOIL_* macros
- [ ] Use SDL functions directly
- [ ] Remove #ifdef USE_SDL blocks

### birth.c
- [ ] Remove #ifdef USE_SDL blocks

### angband.h
- [ ] Remove #ifdef USE_SDL from text_out_file and highscore_fd declarations

## Phase 3: Modernize to C17
- [ ] Replace errr with bool where appropriate
- [ ] Use size_t consistently for sizes
- [ ] Add proper error handling with errno
- [ ] Use const correctness
- [ ] Remove unnecessary casts

## Phase 4: Migrate Remaining Files
- [ ] init1.c, init2.c
- [ ] squelch.c  
- [ ] Any other files using my_fopen or fd_open

## Phase 5: Testing
- [ ] Test save/load
- [ ] Test highscores (multiple entries)
- [ ] Test character dumps
- [ ] Test spoiler generation
