/* File: object/object-inventory-limits.h */

#ifndef INCLUDED_OBJECT_INVENTORY_LIMITS_H
#define INCLUDED_OBJECT_INVENTORY_LIMITS_H

#include "angband.h"

#ifndef INVENTORY_LIMIT_GROUP_DEFINED
#define INVENTORY_LIMIT_GROUP_DEFINED
enum inventory_limit_group
{
    INV_LIMIT_NONE = 0,
    INV_LIMIT_ARROW,
    INV_LIMIT_BOW,
    INV_LIMIT_STAFF,
    INV_LIMIT_HORN,
    INV_LIMIT_DIGGING,
    INV_LIMIT_BOOTS,
    INV_LIMIT_GLOVES,
    INV_LIMIT_HELM_CROWN,
    INV_LIMIT_ROUND_SHIELD,
    INV_LIMIT_OTHER_SHIELD,
    INV_LIMIT_CLOAK,
    INV_LIMIT_SOFT_ARMOUR,
    INV_LIMIT_MAIL,
    INV_LIMIT_MELEE_WEAPON,
    INV_LIMIT_THROWABLE,
    INV_LIMIT_SUPPLY_WEIGHT,
    INV_LIMIT_TORCHES,
    INV_LIMIT_BRASS_LAMPS,
    INV_LIMIT_LESSER_JEWEL,
    INV_LIMIT_FEANORIAN_LAMP
};
#endif

bool inven_carry_limit_failed(void);
enum inventory_limit_group inven_carry_limit_group(void);
cptr inven_carry_limit_label(void);
int inven_carry_limit_value(void);
bool inven_carry_limit_is_supply_weight(void);
bool inven_carry_limit_can_replace(const object_type* o_ptr);
enum inventory_limit_group inventory_limit_group_for_object(
    const object_type* o_ptr);
bool inventory_limit_info_for_object(const object_type* o_ptr,
    enum inventory_limit_group* group, int* limit, int* cost);
int inventory_limit_usage_for_group(enum inventory_limit_group group);
int inventory_limit_limit_for_group(enum inventory_limit_group group);
int inventory_limit_space_for_object(const object_type* o_ptr);
bool inventory_limit_object_matches_group(enum inventory_limit_group group,
    const object_type* o_ptr);
cptr inventory_limit_group_name(enum inventory_limit_group group);
void inven_enforce_current_pack_limits(void);
int object_stack_limit(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_INVENTORY_LIMITS_H */
