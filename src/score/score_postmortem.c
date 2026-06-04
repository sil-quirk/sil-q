/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "score/score_postmortem.h"

static char g_postmortem_scores_path[1024];

void score_postmortem_clear(void)
{
    g_postmortem_scores_path[0] = '\0';
}

void score_postmortem_set_path(const char* path)
{
    if (path && path[0])
        SDL_strlcpy(g_postmortem_scores_path, path, sizeof(g_postmortem_scores_path));
    else
        score_postmortem_clear();
}

const char* score_postmortem_path(void)
{
    return g_postmortem_scores_path;
}