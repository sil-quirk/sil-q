#ifndef INCLUDED_SUPPORT_PROMPT_H
#define INCLUDED_SUPPORT_PROMPT_H

#include "h-basic.h"

bool askfor_aux(char* buf, size_t len);
bool askfor_name(char* buf, size_t len);
bool term_get_string(cptr prompt, char* buf, size_t len);
s16b get_quantity(cptr prompt, int max);
s16b get_quantity_touch_category(cptr prompt, int max, int touch_category);
s16b get_quantity_touch_category_force_prompt(cptr prompt, int max,
    int touch_category);
int get_check_other(cptr prompt, char other);
bool get_check(cptr prompt);
bool get_check_oath_multiline(cptr prompt);
int get_menu_choice(s16b max, char* prompt);
bool get_com(cptr prompt, char* command);
void pause_line(int row);

#endif /* INCLUDED_SUPPORT_PROMPT_H */
