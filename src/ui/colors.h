/* ui/colors.h - Color name and attribute utilities */

#ifndef INCLUDED_UI_COLORS_H
#define INCLUDED_UI_COLORS_H

#include "../h-basic.h"

#define UI_COLOR_PRESET_MAX 16
#define UI_COLOR_PRESET_ID_LEN 64
#define UI_COLOR_PRESET_LABEL_LEN 64

/*
 * Extract a textual representation of an attribute.
 * Returns the base color name with optional shade suffix.
 */
extern cptr attr_to_text(byte a);
extern int color_char_to_attr(char c);
extern int color_text_to_attr(cptr name);
extern cptr get_ext_color_name(byte ext_color);
extern bool ui_colors_load_palette_presets(void);
extern int ui_colors_palette_preset_count(void);
extern cptr ui_colors_palette_preset_id(int index);
extern cptr ui_colors_palette_preset_label(int index);
extern cptr ui_colors_current_palette_preset(void);
extern bool ui_colors_apply_palette_preset(cptr id);

#ifdef SUPPORT_GAMMA
extern void build_gamma_table(int gamma);
extern byte gamma_table[256];
#endif /* SUPPORT_GAMMA */

#endif /* INCLUDED_UI_COLORS_H */
