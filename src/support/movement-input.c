#include "angband.h"
#include "support/movement-input.h"

enum {
    MOVEMENT_INPUT_QUEUE_SIZE = 32
};

typedef struct movement_input_queue_state {
    movement_input_command queued[MOVEMENT_INPUT_QUEUE_SIZE];
    u16b count;
    u16b active_context;
} movement_input_queue_state;

static movement_input_queue_state g_movement_input = { 0 };

static u16b movement_input_allowed_modifiers(void)
{
    return MOVEMENT_INPUT_MODIFIER_SHIFT
        | MOVEMENT_INPUT_MODIFIER_CTRL
        | MOVEMENT_INPUT_MODIFIER_ALT
        | MOVEMENT_INPUT_MODIFIER_META;
}

static bool movement_input_action_is_known(u16b action)
{
    return action <= MOVEMENT_INPUT_ACTION_REST;
}

static bool movement_input_modifiers_match(
    const movement_input_binding* binding, u16b modifiers)
{
    if (!binding)
        return false;

    if ((modifiers & binding->required_modifiers)
        != binding->required_modifiers)
    {
        return false;
    }
    if (modifiers & binding->forbidden_modifiers)
        return false;

    return true;
}

void movement_input_binding_clear(movement_input_binding* binding)
{
    if (!binding)
        return;

    memset(binding, 0, sizeof(*binding));
}

void movement_input_command_clear(movement_input_command* command)
{
    if (!command)
        return;

    memset(command, 0, sizeof(*command));
}

bool movement_input_action_is_directional(u16b action)
{
    return action == MOVEMENT_INPUT_ACTION_MOVE_DIR
        || action == MOVEMENT_INPUT_ACTION_RUN_DIR
        || action == MOVEMENT_INPUT_ACTION_INTERACT_DIR;
}

bool movement_input_context_is_known(u16b context)
{
    return context <= MOVEMENT_INPUT_CONTEXT_TARGETING;
}

bool movement_input_direction_is_known(u16b direction)
{
    return direction <= MOVEMENT_INPUT_DIRECTION_NORTHEAST;
}

bool movement_input_context_matches(u16b command_context, u16b requested_context)
{
    return requested_context == MOVEMENT_INPUT_CONTEXT_ANY
        || command_context == MOVEMENT_INPUT_CONTEXT_ANY
        || command_context == requested_context;
}

bool movement_input_direction_from_legacy_keypad(int keypad_dir,
    u16b* out_direction)
{
    if (keypad_dir < 1 || keypad_dir > 9)
        return false;

    if (out_direction)
        *out_direction = (u16b)keypad_dir;
    return true;
}

int movement_input_direction_to_legacy_keypad(u16b direction)
{
    if (!movement_input_direction_is_known(direction)
        || direction == MOVEMENT_INPUT_DIRECTION_NONE)
    {
        return 0;
    }

    return (int)direction;
}

bool movement_input_binding_is_valid(const movement_input_binding* binding)
{
    if (!binding)
        return false;
    if (!movement_input_context_is_known(binding->context)
        || binding->context == MOVEMENT_INPUT_CONTEXT_NONE)
    {
        return false;
    }
    if (!movement_input_action_is_known(binding->action)
        || binding->action == MOVEMENT_INPUT_ACTION_NONE)
    {
        return false;
    }
    if ((binding->required_modifiers | binding->forbidden_modifiers)
        & ~movement_input_allowed_modifiers())
    {
        return false;
    }
    if (binding->required_modifiers & binding->forbidden_modifiers)
        return false;
    if (!binding->trigger)
        return false;

    if (movement_input_action_is_directional(binding->action))
    {
        if (!movement_input_direction_is_known(binding->direction)
            || binding->direction == MOVEMENT_INPUT_DIRECTION_NONE)
        {
            return false;
        }
    }
    else if (binding->direction != MOVEMENT_INPUT_DIRECTION_NONE
        && binding->direction != MOVEMENT_INPUT_DIRECTION_CENTER)
    {
        return false;
    }

    return true;
}

bool movement_input_binding_matches(const movement_input_binding* binding,
    u16b context, u32b trigger, u32b trigger_aux, u16b modifiers)
{
    if (!movement_input_binding_is_valid(binding))
        return false;
    if (!movement_input_context_matches(binding->context, context))
        return false;
    if (binding->trigger != trigger)
        return false;
    if (binding->trigger_aux != 0 && binding->trigger_aux != trigger_aux)
        return false;
    if (!movement_input_modifiers_match(binding, modifiers))
        return false;

    return true;
}

bool movement_input_bindings_conflict(const movement_input_binding* left,
    const movement_input_binding* right)
{
    u16b supported = movement_input_allowed_modifiers();

    if (!movement_input_binding_is_valid(left)
        || !movement_input_binding_is_valid(right))
    {
        return false;
    }
    if (!movement_input_context_matches(left->context, right->context))
        return false;
    if (left->trigger != right->trigger)
        return false;
    if (left->trigger_aux != 0 && right->trigger_aux != 0
        && left->trigger_aux != right->trigger_aux)
    {
        return false;
    }

    for (u16b modifiers = 0; modifiers <= supported; modifiers++)
    {
        if (modifiers & ~supported)
            continue;
        if (movement_input_modifiers_match(left, modifiers)
            && movement_input_modifiers_match(right, modifiers))
        {
            return true;
        }
    }

    return false;
}

bool movement_input_command_from_binding(const movement_input_binding* binding,
    u16b context, movement_input_command* out_command)
{
    if (!out_command)
        return false;
    if (!movement_input_binding_is_valid(binding))
        return false;

    movement_input_command_clear(out_command);
    out_command->context =
        (binding->context == MOVEMENT_INPUT_CONTEXT_ANY) ? context : binding->context;
    out_command->action = binding->action;
    out_command->direction = binding->direction;

    return movement_input_command_is_valid(out_command);
}

bool movement_input_command_is_valid(const movement_input_command* command)
{
    if (!command)
        return false;
    if (!movement_input_context_is_known(command->context)
        || command->context == MOVEMENT_INPUT_CONTEXT_NONE)
    {
        return false;
    }
    if (!movement_input_action_is_known(command->action)
        || command->action == MOVEMENT_INPUT_ACTION_NONE)
    {
        return false;
    }

    if (movement_input_action_is_directional(command->action))
    {
        if (!movement_input_direction_is_known(command->direction)
            || command->direction == MOVEMENT_INPUT_DIRECTION_NONE)
        {
            return false;
        }
    }
    else if (command->direction != MOVEMENT_INPUT_DIRECTION_NONE
        && command->direction != MOVEMENT_INPUT_DIRECTION_CENTER)
    {
        return false;
    }

    return true;
}

char movement_input_command_legacy_char(
    const movement_input_command* command)
{
    if (!movement_input_command_is_valid(command))
        return '\0';

    switch (command->action)
    {
    case MOVEMENT_INPUT_ACTION_MOVE_DIR:
        return ';';
    case MOVEMENT_INPUT_ACTION_RUN_DIR:
        return '.';
    case MOVEMENT_INPUT_ACTION_INTERACT_DIR:
        return '/';
    case MOVEMENT_INPUT_ACTION_WAIT:
        return 'z';
    case MOVEMENT_INPUT_ACTION_REST:
        return 'Z';
    default:
        return '\0';
    }
}

int movement_input_command_legacy_dir(
    const movement_input_command* command)
{
    if (!movement_input_command_is_valid(command))
        return 0;
    if (!movement_input_action_is_directional(command->action))
        return 0;

    return movement_input_direction_to_legacy_keypad(command->direction);
}

bool movement_input_submit_command(const movement_input_command* command)
{
    if (!movement_input_command_is_valid(command))
        return false;
    if (g_movement_input.count >= MOVEMENT_INPUT_QUEUE_SIZE)
        return false;

    g_movement_input.queued[g_movement_input.count++] = *command;
    return true;
}

bool movement_input_take_command(u16b context,
    movement_input_command* out_command)
{
    u16b i;

    for (i = 0; i < g_movement_input.count; i++)
    {
        if (!movement_input_context_matches(
                g_movement_input.queued[i].context, context))
        {
            continue;
        }

        if (out_command)
            *out_command = g_movement_input.queued[i];

        if (i + 1 < g_movement_input.count)
        {
            memmove(&g_movement_input.queued[i],
                &g_movement_input.queued[i + 1],
                (g_movement_input.count - (i + 1))
                    * sizeof(g_movement_input.queued[0]));
        }
        g_movement_input.count--;
        return true;
    }

    return false;
}

static int movement_input_command_to_legacy_dir(
    const movement_input_command* command)
{
    if (movement_input_action_is_directional(command->action))
        return movement_input_direction_to_legacy_keypad(command->direction);
    if (command->action == MOVEMENT_INPUT_ACTION_WAIT)
        return 5;

    return 0;
}

bool movement_input_take_legacy_direction(u16b context, int* out_dir)
{
    u16b i;

    /*
     * Only consume a command that actually yields a usable legacy direction.
     * Commands that cannot (e.g. REST in a direction prompt) are left in the
     * queue rather than silently eaten, so callers do not lose input.
     */
    for (i = 0; i < g_movement_input.count; i++)
    {
        const movement_input_command* command = &g_movement_input.queued[i];
        int dir;

        if (!movement_input_context_matches(command->context, context))
            continue;

        dir = movement_input_command_to_legacy_dir(command);
        if (!dir)
            continue;

        if (i + 1 < g_movement_input.count)
        {
            memmove(&g_movement_input.queued[i],
                &g_movement_input.queued[i + 1],
                (g_movement_input.count - (i + 1))
                    * sizeof(g_movement_input.queued[0]));
        }
        g_movement_input.count--;

        if (out_dir)
            *out_dir = dir;
        return true;
    }

    return false;
}

void movement_input_clear_commands(void)
{
    g_movement_input.count = 0;
}

u16b movement_input_active_context(void)
{
    return g_movement_input.active_context;
}

void movement_input_set_active_context(u16b context)
{
    if (!movement_input_context_is_known(context))
        context = MOVEMENT_INPUT_CONTEXT_NONE;

    g_movement_input.active_context = context;
}
