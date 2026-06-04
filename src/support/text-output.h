#ifndef INCLUDED_SUPPORT_TEXT_OUTPUT_H
#define INCLUDED_SUPPORT_TEXT_OUTPUT_H

#include "h-basic.h"
#include <SDL3/SDL.h>

extern SDL_IOStream* text_out_file;
extern void (*text_out_hook)(byte a, cptr str);
extern int text_out_wrap;
extern int text_out_indent;

int count_wrapped_lines(cptr str, int wrap_width, int indent);
void text_out_to_screen(byte a, cptr str);
void text_out_to_file(byte a, cptr str);
void text_out(cptr str);
void text_out_c(byte a, cptr str);

#endif /* INCLUDED_SUPPORT_TEXT_OUTPUT_H */
