#ifndef INCLUDED_SUPPORT_PROMPT_H
#define INCLUDED_SUPPORT_PROMPT_H

#include "h-basic.h"

bool askfor_aux(char* buf, size_t len);
bool askfor_name(char* buf, size_t len);
bool term_get_string(cptr prompt, char* buf, size_t len);
bool get_string_panel(cptr prompt, char* buf, size_t len);
s16b get_quantity(cptr prompt, int max);
s16b get_quantity_action(cptr prompt, cptr action, int max);
s16b get_quantity_touch_category(cptr prompt, int max, int touch_category);
s16b get_quantity_touch_category_action(cptr prompt, cptr action, int max,
    int touch_category);
s16b get_quantity_touch_category_force_prompt(cptr prompt, int max,
    int touch_category);
s16b get_quantity_touch_category_force_prompt_action(cptr prompt, cptr action,
    int max, int touch_category);
bool get_check(cptr prompt);
bool get_check_lower(cptr prompt);
bool get_check_near(int y, int x, cptr prompt);
bool get_check_oath_multiline(cptr prompt);
bool get_com(cptr prompt, char* command);
void any_key_prompt_text(char* buf, size_t len, cptr action);
void pause_line(int row);
bool terminal_prompt_fits(cptr prompt, int max_width, bool use_story_font);
void terminal_prompt_trim(char* prompt, int max_width, bool use_story_font);
void terminal_prompt_pick_variant(char* out, size_t out_size, int max_width,
    bool use_story_font, const char* const variants[], size_t variant_count);
void terminal_prompt_put_variant(int col, int row, int max_width, byte attr,
    bool use_story_font, const char* const variants[], size_t variant_count);

#endif /* INCLUDED_SUPPORT_PROMPT_H */
