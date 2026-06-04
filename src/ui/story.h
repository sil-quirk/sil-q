#ifndef INCLUDED_UI_STORY_H
#define INCLUDED_UI_STORY_H

#include "h-basic.h"

void print_story(int last_parts, bool fade_in);
void print_fade_line(cptr text, int row, int indent);
void print_fade_centered(cptr text);
void print_fade_centered_at_row(cptr text, int row_start, bool fade_in,
    bool line_delay);

#endif /* INCLUDED_UI_STORY_H */