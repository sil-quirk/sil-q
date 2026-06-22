#ifndef SIL_UI_TARGETING_INTERNAL_H
#define SIL_UI_TARGETING_INTERNAL_H

#include "angband.h"

void target_prompt_label(
    int binding, cptr fallback, char* buf, size_t buflen);

/* Pick the "temp" array entry nearest to (y1,x1) in direction (dy,dx) */
s16b target_pick(int y1, int x1, int dy, int dx);

/* Interactive aim selection for fire/throw/aim commands.
 * On success sets *dp to 5 (target chosen) or DIRECTION_UP/DOWN. */
bool target_select_aim(int range, bool allow_vertical, int* dp);

/* Pick any dungeon grid with keyboard, controller, mouse, or touch input. */
bool target_select_location(cptr action, int* y, int* x);

#endif
