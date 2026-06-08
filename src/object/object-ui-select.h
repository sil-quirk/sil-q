/* File: object/object-ui-select.h */

#ifndef INCLUDED_OBJECT_UI_SELECT_H
#define INCLUDED_OBJECT_UI_SELECT_H

#include "angband.h"

char index_to_label(int i);
s16b label_to_inven(int c);
s16b label_to_equip(int c);
s16b wield_slot(const object_type* o_ptr);
cptr describe_empty_slot(int i);
cptr mention_use(int i);
cptr describe_use(int i);
bool object_is_searched_skeleton(const object_type* o_ptr);
bool item_tester_okay(const object_type* o_ptr);
int scan_floor(int* items, int size, int y, int x, int mode);
bool get_item_okay(int item);
bool get_item_allow(int item);
bool get_item(int* cp, cptr pmt, cptr str, int mode);

#endif /* INCLUDED_OBJECT_UI_SELECT_H */
