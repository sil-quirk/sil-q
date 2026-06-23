/* File: spell/spell-identify.h */

#ifndef INCLUDED_SPELL_IDENTIFY_H
#define INCLUDED_SPELL_IDENTIFY_H

#include "../h-basic.h"

typedef struct object_type object_type;

bool item_tester_hook_digger(const object_type* o_ptr);
bool item_tester_hook_ided_weapon(const object_type* o_ptr);
bool item_tester_hook_weapon(const object_type* o_ptr);
bool item_tester_hook_wieldable_ided_weapon(const object_type* o_ptr);
bool item_tester_hook_wieldable_weapon(const object_type* o_ptr);
bool item_tester_hook_ided_armour(const object_type* o_ptr);
bool item_tester_hook_armour(const object_type* o_ptr);
bool item_tester_hook_non_herb_food(const object_type* o_ptr);
bool item_tester_hook_light_with_fuel(const object_type* o_ptr);
bool item_tester_hook_enchantable_amulet(const object_type* o_ptr);
bool ident_spell(bool include_floor);
bool item_tester_hook_recharge(const object_type* o_ptr);
bool recharge(int num);
bool item_tester_hook_ided_ammo(const object_type* o_ptr);
bool item_tester_hook_ammo(const object_type* o_ptr);
bool item_tester_hook_ordinary_ammo(const object_type* o_ptr);
void identify_and_describe_pack(void);
bool mass_identify(int rad);
void do_ident_item(int item, object_type* o_ptr);

#endif /* INCLUDED_SPELL_IDENTIFY_H */
