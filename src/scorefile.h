#ifndef INCLUDED_SCOREFILE_H
#define INCLUDED_SCOREFILE_H

#include "h-basic.h"
#include <SDL3/SDL.h>

typedef struct score_file_ctx {
    SDL_IOStream* fd;
    byte version_major;
    byte version_minor;
    byte version_patch;
    byte version_extra;
    u32b entry_count;
} score_file_ctx;

/* Swap the active score-file context; returns the previous context pointer. */
extern score_file_ctx* score_file_set_active_ctx(score_file_ctx* ctx);

/* Returns the currently active context (global by default). */
extern score_file_ctx* score_file_active_ctx(void);

/* Convenience helper for zeroing a context before use. */
extern void score_file_reset_ctx(score_file_ctx* ctx);

#endif /* INCLUDED_SCOREFILE_H */
