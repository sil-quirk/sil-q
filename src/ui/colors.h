/* ui/colors.h - Color name and attribute utilities */

#ifndef INCLUDED_UI_COLORS_H
#define INCLUDED_UI_COLORS_H

#include "../h-basic.h"

/*
 * Extract a textual representation of an attribute.
 * Returns the base color name with optional shade suffix.
 */
extern cptr attr_to_text(byte a);

#endif /* INCLUDED_UI_COLORS_H */
