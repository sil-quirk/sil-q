#include "score/score_io.h"

#include <string.h>

static score_file_ctx global_score_ctx;
static score_file_ctx* active_score_ctx = &global_score_ctx;

score_file_ctx* score_file_set_active_ctx(score_file_ctx* ctx)
{
    score_file_ctx* previous = active_score_ctx;
    active_score_ctx = ctx ? ctx : &global_score_ctx;
    return previous;
}

score_file_ctx* score_file_active_ctx(void)
{
    return active_score_ctx;
}

score_file_ctx* score_file_global_ctx(void)
{
    return &global_score_ctx;
}

bool scores_version_has_curses(const score_file_ctx* ctx)
{
    if (!ctx)
        return false;

    /* Compare version tuple: major.minor.patch.extra */
    if (ctx->version_major > 0)
        return true;
    if (ctx->version_major < 0)
        return false;

    if (ctx->version_minor > 9)
        return true;
    if (ctx->version_minor < 9)
        return false;

    if (ctx->version_patch > 0)
        return true;
    if (ctx->version_patch < 0)
        return false;

    return (ctx->version_extra >= 6);
}

void score_file_reset_ctx(score_file_ctx* ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}
