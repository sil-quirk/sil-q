/* File: object/object-ui-display.h */

#ifndef INCLUDED_OBJECT_UI_DISPLAY_H
#define INCLUDED_OBJECT_UI_DISPLAY_H

#include "angband.h"

bool inventory_menu_set_include_equip(bool include);
bool inventory_menu_set_expand_supplies(bool enabled);
byte object_attr_graphics_override(const object_type* o_ptr, byte base_attr);
char object_char_graphics_override(const object_type* o_ptr, char base_char);
void display_inven(void);
void display_equip(void);
void display_supplies(void);
void show_inven(void);
void show_equip(void);
void show_floor(const int* floor_list, int floor_num);
void toggle_inven_equip(void);

#endif /* INCLUDED_OBJECT_UI_DISPLAY_H */
