#ifndef INCLUDED_SCORE_IO_H
#define INCLUDED_SCORE_IO_H

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
score_file_ctx* score_file_set_active_ctx(score_file_ctx* ctx);

/* Returns the currently active context (global by default). */
score_file_ctx* score_file_active_ctx(void);

/* Accessor for the module's default/global context. */
score_file_ctx* score_file_global_ctx(void);

/* Utility: check if a score file version supports the curse (pts) field. */
bool scores_version_has_curses(const score_file_ctx* ctx);

/* Convenience helper for zeroing a context before use. */
void score_file_reset_ctx(score_file_ctx* ctx);

#endif /* INCLUDED_SCORE_IO_H */
