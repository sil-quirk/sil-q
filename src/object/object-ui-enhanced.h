/* File: object/object-ui-enhanced.h */

#ifndef INCLUDED_OBJECT_UI_ENHANCED_H
#define INCLUDED_OBJECT_UI_ENHANCED_H

#include "angband.h"

void describe_item_with_comparisons(int item_index, bool include_comparisons);
char describe_item_with_floor_actions(int item_index, bool include_comparisons);
void show_inven_enhanced(void);
void show_equip_enhanced(void);

#endif /* INCLUDED_OBJECT_UI_ENHANCED_H */
