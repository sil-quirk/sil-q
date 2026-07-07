/* File: object/object-ui-select.h */

#ifndef INCLUDED_OBJECT_UI_SELECT_H
#define INCLUDED_OBJECT_UI_SELECT_H

#include "angband.h"

#define OBJECT_CHOICE_TEXT_LEN 96
#define OBJECT_CHOICE_LABEL_LEN 8
#define OBJECT_CHOICE_MAX_ENTRIES 320

typedef struct object_choice_entry
{
    int item;
    object_type* o_ptr;
    char label[OBJECT_CHOICE_LABEL_LEN];
    char text[OBJECT_CHOICE_TEXT_LEN];
    byte attr;
} object_choice_entry;

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
void object_choice_entry_make(object_choice_entry* entry, int item,
    object_type* o_ptr, cptr label, cptr prefix);
bool object_choice_overlay(cptr title, cptr desc,
    const object_choice_entry entries[], int count, int default_index,
    int* out_entry);
bool object_item_select_overlay(int mode, cptr reason, cptr none_msg,
    int* item_out);

#endif /* INCLUDED_OBJECT_UI_SELECT_H */
