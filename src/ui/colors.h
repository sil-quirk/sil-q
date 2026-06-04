/* ui/colors.h - Color name and attribute utilities */

#ifndef INCLUDED_UI_COLORS_H
#define INCLUDED_UI_COLORS_H

#include "../h-basic.h"

/*
 * Extract a textual representation of an attribute.
 * Returns the base color name with optional shade suffix.
 */
extern cptr attr_to_text(byte a);
extern int color_char_to_attr(char c);
extern int color_text_to_attr(cptr name);
extern cptr get_ext_color_name(byte ext_color);

#ifdef SUPPORT_GAMMA
extern void build_gamma_table(int gamma);
extern byte gamma_table[256];
#endif /* SUPPORT_GAMMA */

#endif /* INCLUDED_UI_COLORS_H */
