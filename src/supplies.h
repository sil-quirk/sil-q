#ifndef SUPPLIES_H
#define SUPPLIES_H

#include "h-basic.h"
#define SUPPLIES_MAX_WEIGHT_DEFAULT 250 /* 25 lbs expressed in tenths */
#define SUPPLIES_MAX_WEIGHT_BLESSING 500 /* 50 lbs expressed in tenths */

struct object_type;

typedef enum supply_group
{
    SUPPLY_GROUP_HERBS = 0,
    SUPPLY_GROUP_FOOD,
    SUPPLY_GROUP_POTIONS,
    SUPPLY_GROUP_GEMS,
    SUPPLY_GROUP_LIGHTS,
    SUPPLY_GROUP_MAX
} supply_group;

#define SUPPLIES_INDEX 1000
#define PLAYER_TORCH_CAP 5
#define PLAYER_OIL_CONTAINER_SLOT_CAP 4
#define PLAYER_BRASS_LAMP_SLOT_COST 2
#define PLAYER_OIL_FLASK_SLOT_COST 1
#define PLAYER_BRASS_LAMP_CAP 2
#define PLAYER_PERMANENT_LIGHT_CAP 3
#define PLAYER_LAMP_OIL_MAX 10000

typedef enum supply_menu_action
{
    SUPPLY_MENU_ACTION_NONE = 0,
    SUPPLY_MENU_ACTION_USE,
    SUPPLY_MENU_ACTION_DROP
} supply_menu_action;

typedef struct supply_menu_request
{
    bool focus_group;          /* jump to specific group */
    int group;                 /* supply_group value */
    supply_menu_action action; /* default action when selecting */
    bool hotkey_mode;          /* close menu after first action */
} supply_menu_request;

void supplies_init(void);
void supplies_dispose(void);
void supplies_reset_store(void);

void supplies_set_allow_overflow(bool allow);
void supplies_set_max_weight_cap(int weight_tenths);
int supplies_current_weight_cap(void);
int supplies_limit_weight(void);
bool supplies_weight_counts_to_limit(const struct object_type* o_ptr);
bool supplies_is_supply_object(const struct object_type* o_ptr);
bool supplies_is_herb_object(const struct object_type* o_ptr);
bool supplies_is_food_object(const struct object_type* o_ptr);
bool supplies_is_light_kind(int sval);
bool supplies_is_light_object(const struct object_type* o_ptr);
bool supplies_group_matches_kind(int group, int tval, int sval);
bool supplies_group_matches_object(int group, const struct object_type* o_ptr);
bool supplies_is_carried_object_pointer(const struct object_type* o_ptr);

/* Returns true if object can be added without exceeding weight limit. */
bool supplies_can_absorb_object(const struct object_type* o_ptr);

/* Returns maximum quantity that can be absorbed within weight limit. */
int supplies_max_absorbable_quantity(const struct object_type* o_ptr);

/* Returns true when absorbed (src is wiped). */
bool supplies_absorb_object(struct object_type* src);

int supplies_entry_count(void);
struct object_type* supplies_entry_at(int idx);
int supplies_entry_units(int idx);
int supplies_first_entry_for_kind(int k_idx);
bool supplies_take_one(int idx, struct object_type* out);

int supplies_total_weight(void);
int supplies_carried_light_item_weight(void);
void supplies_count_totals(int* herbs, int* food, int* potions, int* gems,
    int* lights);

int player_light_max_fuel(const struct object_type* o_ptr);
int player_light_sputter_threshold(const struct object_type* o_ptr);
bool player_light_uses_oil_pool(const struct object_type* o_ptr);
int player_light_fuel(const struct object_type* o_ptr);
bool player_light_has_fuel(const struct object_type* o_ptr);
bool player_light_destroyed_on_drop(const struct object_type* o_ptr);
void player_light_set_fuel(struct object_type* o_ptr, int fuel);
void player_light_add_fuel(struct object_type* o_ptr, int amount);
int player_lamp_oil_capacity(void);
int player_lamp_oil_capacity_with_bonus(int lantern_bonus);
int player_lamp_oil(void);
int player_lamp_oil_weight(void);
void player_set_lamp_oil(int oil);
bool player_oil_container_object(const struct object_type* o_ptr);
int player_oil_container_slot_cost(const struct object_type* o_ptr);
int player_oil_container_unit_capacity(const struct object_type* o_ptr);
int player_oil_container_slots_used(void);
int player_oil_container_slot_capacity(void);
void player_oil_container_set_fuel(struct object_type* o_ptr, int fuel);
int player_refill_lamp_oil_from_container(struct object_type* o_ptr);
bool player_lamp_oil_would_overflow(int addition);
bool player_lamp_oil_would_overflow_with_bonus(int addition, int lantern_bonus);
bool player_gain_lamp_oil(int addition, bool allow_overflow);
bool player_gain_lamp_oil_with_bonus(int addition, bool allow_overflow,
    int lantern_bonus);
bool player_prepare_lantern_drop(int lanterns_being_dropped,
    int* oil_to_transfer, int* oil_to_lose);
bool player_prepare_oil_container_drop(const struct object_type* o_ptr,
    int amount, int* oil_to_transfer, int* oil_to_lose);
bool player_prepare_oil_container_drop_after_removal(
    const struct object_type* o_ptr, int amount, int* oil_to_transfer,
    int* oil_to_lose);
int player_carried_torch_count(void);
int player_carried_light_count_for_sval(int sval);
int player_light_carry_group(const struct object_type* o_ptr);
bool player_light_share_carry_group(const struct object_type* first,
    const struct object_type* second);
int player_light_carry_cap(const struct object_type* o_ptr);
int player_light_available_capacity(const struct object_type* o_ptr);
void player_light_reserve_incoming(const struct object_type* o_ptr, int amount);
void player_light_clear_incoming_reservation(void);

bool supplies_has_group(int group);
int supplies_first_entry_for_group(int group);

char supplies_label_char(void);
int supplies_virtual_slot(void);

bool supplies_consume_quantity(int idx, int amount);
void supplies_refresh_entry(int idx);
bool supplies_drop_amount(int idx, int amount);

void supplies_ingest_pack(void);

void supplies_begin_action(int supply_idx);
void supplies_end_action(void);
int supplies_current_action(void);

bool supplies_any_match_item_tester(void);

void supplies_set_pending_action(supply_menu_action action, int group, bool hotkey);
void supplies_clear_pending_action(void);
bool supplies_has_pending_action(void);
supply_menu_action supplies_pending_action(void);
int supplies_pending_group(void);
bool supplies_pending_hotkey(void);

/* Damage supply items (similar to inven_damage) */
int supplies_damage(int (*typ)(const struct object_type*), int perc, int resistance);
int supplies_damage_cold(int perc, int resistance);

#endif /* SUPPLIES_H */
