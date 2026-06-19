#ifndef INCLUDED_UI_STORY_H
#define INCLUDED_UI_STORY_H

#include "h-basic.h"

void print_story(int last_parts, bool fade_in);
void print_fade_line(cptr text, int row, int indent);
void print_fade_centered(cptr text);

#endif /* INCLUDED_UI_STORY_H */
