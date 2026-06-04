/* File: object/object-inventory.h */

#ifndef INCLUDED_OBJECT_INVENTORY_H
#define INCLUDED_OBJECT_INVENTORY_H

#include "angband.h"

void inven_item_charges(int item);
void inven_item_describe(int item);
void inven_item_increase(int item, int num);
void inven_item_optimize(int item);
void floor_item_charges(int item);
void floor_item_describe(int item);
void floor_item_increase(int item, int num);
void floor_item_optimize(int item);
void check_pack_overflow(void);
bool inven_carry_okay(const object_type* o_ptr);
bool inven_carry_okay_after_removing(const object_type* o_ptr,
    int remove_item, int remove_amt);
s16b inven_carry(object_type* o_ptr, bool combine_ammo);
s16b inven_takeoff(int item, int amt);
void inven_drop(int item, int amt);
void combine_pack(void);
void reorder_pack(bool display_message);
void check_artifact_visibility(void);

#endif /* INCLUDED_OBJECT_INVENTORY_H */
