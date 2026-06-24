#ifndef INCLUDED_SUPPORT_MOVEMENT_INPUT_H
#define INCLUDED_SUPPORT_MOVEMENT_INPUT_H

#include "h-basic.h"

#define MOVEMENT_INPUT_FORMAT_VERSION 1u

typedef enum movement_input_context {
    MOVEMENT_INPUT_CONTEXT_NONE = 0,
    MOVEMENT_INPUT_CONTEXT_ANY = 1,
    MOVEMENT_INPUT_CONTEXT_DUNGEON = 2,
    MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT = 3,
    MOVEMENT_INPUT_CONTEXT_TARGETING = 4
} movement_input_context;

typedef enum movement_input_action {
    MOVEMENT_INPUT_ACTION_NONE = 0,
    MOVEMENT_INPUT_ACTION_MOVE_DIR = 1,
    MOVEMENT_INPUT_ACTION_RUN_DIR = 2,
    MOVEMENT_INPUT_ACTION_INTERACT_DIR = 3,
    MOVEMENT_INPUT_ACTION_WAIT = 4,
    MOVEMENT_INPUT_ACTION_REST = 5
} movement_input_action;

typedef enum movement_input_direction {
    MOVEMENT_INPUT_DIRECTION_NONE = 0,
    MOVEMENT_INPUT_DIRECTION_SOUTHWEST = 1,
    MOVEMENT_INPUT_DIRECTION_SOUTH = 2,
    MOVEMENT_INPUT_DIRECTION_SOUTHEAST = 3,
    MOVEMENT_INPUT_DIRECTION_WEST = 4,
    MOVEMENT_INPUT_DIRECTION_CENTER = 5,
    MOVEMENT_INPUT_DIRECTION_EAST = 6,
    MOVEMENT_INPUT_DIRECTION_NORTHWEST = 7,
    MOVEMENT_INPUT_DIRECTION_NORTH = 8,
    MOVEMENT_INPUT_DIRECTION_NORTHEAST = 9
} movement_input_direction;

typedef enum movement_input_modifier {
    MOVEMENT_INPUT_MODIFIER_SHIFT = 0x0001u,
    MOVEMENT_INPUT_MODIFIER_CTRL = 0x0002u,
    MOVEMENT_INPUT_MODIFIER_ALT = 0x0004u,
    MOVEMENT_INPUT_MODIFIER_META = 0x0008u
} movement_input_modifier;

typedef struct movement_input_binding {
    u16b context;
    u16b action;
    u16b direction;
    u16b required_modifiers;
    u16b forbidden_modifiers;
    u16b reserved;
    u32b trigger;
    u32b trigger_aux;
} movement_input_binding;

typedef struct movement_input_command {
    u16b context;
    u16b action;
    u16b direction;
    u16b flags;
} movement_input_command;

void movement_input_binding_clear(movement_input_binding* binding);
void movement_input_command_clear(movement_input_command* command);

bool movement_input_action_is_directional(u16b action);
bool movement_input_context_is_known(u16b context);
bool movement_input_direction_is_known(u16b direction);
bool movement_input_context_matches(u16b command_context, u16b requested_context);

bool movement_input_direction_from_legacy_keypad(int keypad_dir,
    u16b* out_direction);
int movement_input_direction_to_legacy_keypad(u16b direction);

bool movement_input_binding_is_valid(const movement_input_binding* binding);
bool movement_input_binding_matches(const movement_input_binding* binding,
    u16b context, u32b trigger, u32b trigger_aux, u16b modifiers);
bool movement_input_bindings_conflict(const movement_input_binding* left,
    const movement_input_binding* right);
bool movement_input_command_from_binding(const movement_input_binding* binding,
    u16b context, movement_input_command* out_command);
bool movement_input_command_is_valid(const movement_input_command* command);

char movement_input_command_legacy_char(
    const movement_input_command* command);
int movement_input_command_legacy_dir(
    const movement_input_command* command);

bool movement_input_submit_command(const movement_input_command* command);
bool movement_input_take_command(u16b context,
    movement_input_command* out_command);
bool movement_input_take_legacy_direction(u16b context, int* out_dir);
void movement_input_clear_commands(void);

u16b movement_input_active_context(void);
void movement_input_set_active_context(u16b context);

#endif /* INCLUDED_SUPPORT_MOVEMENT_INPUT_H */
