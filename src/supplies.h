#ifndef SUPPLIES_H
#define SUPPLIES_H

#include "h-basic.h"
#define SUPPLIES_MAX_WEIGHT_DEFAULT 250 /* 25 lbs expressed in tenths */
#define SUPPLIES_MAX_WEIGHT_BLESSING 500 /* 50 lbs expressed in tenths */

struct object_type;

typedef enum supply_group
{
    SUPPLY_GROUP_HERBS = 0,
    SUPPLY_GROUP_POTIONS,
    SUPPLY_GROUP_GEMS,
    SUPPLY_GROUP_MAX
} supply_group;

#define SUPPLIES_INDEX 1000

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
bool supplies_is_supply_object(const struct object_type* o_ptr);

/* Returns true if object can be added without exceeding weight limit. */
bool supplies_can_absorb_object(const struct object_type* o_ptr);

/* Returns maximum quantity that can be absorbed within weight limit. */
int supplies_max_absorbable_quantity(const struct object_type* o_ptr);

/* Returns true when absorbed (src is wiped). */
bool supplies_absorb_object(struct object_type* src);

int supplies_entry_count(void);
struct object_type* supplies_entry_at(int idx);
int supplies_entry_units(int idx);

int supplies_total_weight(void);
void supplies_count_totals(int* potions, int* herbs, int* gems);

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

#endif /* SUPPLIES_H */
