#include "angband.h"
#include "externs.h"
#include "sdl-config.h"
#include "log/log.h"
#include "pane.h"
#include "cJSON.h"
#include <SDL3/SDL_keyboard.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON-based configuration system using cJSON library

typedef struct sdl_default_keymap_entry {
    byte mode;
    byte key;
    const char* action;
} sdl_default_keymap_entry;

static const sdl_default_keymap_entry sdl_default_keymaps[] = {
    { 0, 0x35, "z" },
    { 0, 0x2C, "/5" },
    { 0, 0x20, "/5" },
    { 0, 0x1A, "/5" },
    { 0, 0x31, ";1" },
    { 0, 0x32, ";2" },
    { 0, 0x33, ";3" },
    { 0, 0x34, ";4" },
    { 0, 0x36, ";6" },
    { 0, 0x37, ";7" },
    { 0, 0x38, ";8" },
    { 0, 0x39, ";9" },
    { 1, 0x35, "z" },
    { 1, 0x2C, "/5" },
    { 1, 0x20, "/5" },
    { 1, 0x1A, "/5" },
    { 1, 0x31, ";1" },
    { 1, 0x32, ";2" },
    { 1, 0x33, ";3" },
    { 1, 0x34, ";4" },
    { 1, 0x36, ";6" },
    { 1, 0x37, ";7" },
    { 1, 0x38, ";8" },
    { 1, 0x39, ";9" },
    { 1, 0x62, ";1" },
    { 1, 0x6A, ";2" },
    { 1, 0x6E, ";3" },
    { 1, 0x68, ";4" },
    { 1, 0x6C, ";6" },
    { 1, 0x79, ";7" },
    { 1, 0x6B, ";8" },
    { 1, 0x75, ";9" },
    { 1, 0x42, ".1" },
    { 1, 0x4A, ".2" },
    { 1, 0x4E, ".3" },
    { 1, 0x48, ".4" },
    { 1, 0x4C, ".6" },
    { 1, 0x59, ".7" },
    { 1, 0x4B, ".8" },
    { 1, 0x55, ".9" },
    { 1, 0x02, "b" },
    { 1, 0x0C, "l" },
    { 1, 0x0B, "k" },
    { 1, 0x15, "u" },
    { 1, 0x0E, "n" },
    { 1, 0x57, "L" },
    { 2, 0x35, "z" },
    { 2, 0x2C, "/5" },
    { 2, 0x20, "/5" },
    { 2, 0x1A, "/5" },
    { 2, 0x31, ";1" },
    { 2, 0x32, ";2" },
    { 2, 0x33, ";3" },
    { 2, 0x34, ";4" },
    { 2, 0x36, ";6" },
    { 2, 0x37, ";7" },
    { 2, 0x38, ";8" },
    { 2, 0x39, ";9" },
    { 2, 0x49, "x" },
    { 2, 0x76, "t" },
    { 2, 0x16, "\x14" },
    { 2, 0x55, "u" },
    { 2, 0x74, "r" },
    { 2, 0x75, "a" },
    { 2, 0x42, "b" },
    { 2, 0x73, "z" },
    { 2, 0x52, "Z" },
    { 2, 0x61, "s" },
    { 2, 0x43, "@" },
    { 2, 0x3D, "O" },
    { 2, 0x2B, "/" },
    { 2, 0x40, "$" },
    { 2, 0x78, "A" },
    { 2, 0x72, "A" },
    { 2, 0x62, "A" },
    { 2, 0x5A, "A" },
    { 2, 0x4F, "A" },
    { 2, 0x2F, "A" },
    { 2, 0x14, "A" },
    { 3, 0x35, "z" },
    { 3, 0x2C, "/5" },
    { 3, 0x20, "/5" },
    { 3, 0x1A, "/5" },
    { 3, 0x31, ";1" },
    { 3, 0x32, ";2" },
    { 3, 0x33, ";3" },
    { 3, 0x34, ";4" },
    { 3, 0x36, ";6" },
    { 3, 0x37, ";7" },
    { 3, 0x38, ";8" },
    { 3, 0x39, ";9" },
    { 3, 0x62, ";1" },
    { 3, 0x6A, ";2" },
    { 3, 0x6E, ";3" },
    { 3, 0x68, ";4" },
    { 3, 0x6C, ";6" },
    { 3, 0x79, ";7" },
    { 3, 0x6B, ";8" },
    { 3, 0x75, ";9" },
    { 3, 0x42, ".1" },
    { 3, 0x4A, ".2" },
    { 3, 0x4E, ".3" },
    { 3, 0x48, ".4" },
    { 3, 0x4C, ".6" },
    { 3, 0x59, ".7" },
    { 3, 0x4B, ".8" },
    { 3, 0x55, ".9" },
    { 3, 0x02, "b" },
    { 3, 0x0C, "l" },
    { 3, 0x0B, "k" },
    { 3, 0x15, "u" },
    { 3, 0x0E, "n" },
    { 3, 0x57, "L" },
    { 3, 0x49, "x" },
    { 3, 0x76, "t" },
    { 3, 0x16, "\x14" },
    { 3, 0x74, "r" },
    { 3, 0x73, "z" },
    { 3, 0x52, "Z" },
    { 3, 0x61, "s" },
    { 3, 0x43, "@" },
    { 3, 0x3D, "O" },
    { 3, 0x2B, "/" },
    { 3, 0x40, "$" },
    { 3, 0x78, "A" },
    { 3, 0x72, "A" },
    { 3, 0x5A, "A" },
    { 3, 0x4F, "A" },
    { 3, 0x2F, "A" },
    { 3, 0x14, "A" },
};

static void sdl_config_set_default_keymaps(struct sdl_config* config)
{
    if (!config)
        return;

    memset(config->keymap_actions, 0, sizeof(config->keymap_actions));
    for (int i = 0; i < (int)N_ELEMENTS(sdl_default_keymaps); i++)
    {
        const sdl_default_keymap_entry* entry = &sdl_default_keymaps[i];

        SDL_strlcpy(config->keymap_actions[entry->mode][entry->key],
            entry->action,
            sizeof(config->keymap_actions[entry->mode][entry->key]));
    }
}

void sdl_config_apply_keyboard_keymaps(const struct sdl_config* config)
{
    if (!config)
        return;

    for (int mode = 0; mode < KEYMAP_MODES; mode++)
    {
        for (int key = 0; key < 256; key++)
        {
            keymap_act[mode][key] = str_free(keymap_act[mode][key]);
            if (config->keymap_actions[mode][key][0])
            {
                keymap_act[mode][key] =
                    str_dup(config->keymap_actions[mode][key]);
            }
        }
    }

    /*
     * The keyboard movement preset is intentionally decoupled from the
     * OPT_hjkl_movement game option.  The semantic movement layer handles
     * the Vi keys (h/j/k/l/y/u/b/n) directly through its own bindings, so
     * the preset never needs to drive that option.  Forcing the option from
     * the preset here used to clobber the player's persisted choice on every
     * apply (startup and after each movement-menu edit); leave it owned by
     * the option system instead.
     */
}

static void sdl_config_load_keyboard_keymaps(cJSON* root,
    struct sdl_config* config)
{
    cJSON* keyboard;
    cJSON* modes;

    if (!root || !config)
        return;

    keyboard = cJSON_GetObjectItemCaseSensitive(root, "keyboard");
    if (!cJSON_IsObject(keyboard))
        return;

    modes = cJSON_GetObjectItemCaseSensitive(keyboard, "keymaps");
    if (!cJSON_IsArray(modes))
        return;

    memset(config->keymap_actions, 0, sizeof(config->keymap_actions));

    for (int mode = 0;
        mode < cJSON_GetArraySize(modes) && mode < SDL_KEYMAP_MODE_COUNT;
        mode++)
    {
        cJSON* entries = cJSON_GetArrayItem(modes, mode);
        cJSON* entry = NULL;

        if (!cJSON_IsArray(entries))
            continue;

        cJSON_ArrayForEach(entry, entries)
        {
            cJSON* key = cJSON_GetObjectItemCaseSensitive(entry, "key");
            cJSON* action = cJSON_GetObjectItemCaseSensitive(entry, "action");

            if (!cJSON_IsNumber(key) || !cJSON_IsString(action)
                || key->valueint < 0 || key->valueint >= SDL_KEYMAP_KEY_COUNT
                || !action->valuestring)
            {
                continue;
            }

            SDL_strlcpy(config->keymap_actions[mode][key->valueint],
                action->valuestring,
                sizeof(config->keymap_actions[mode][key->valueint]));
        }
    }
}

static cJSON* sdl_config_create_keyboard_keymaps(void)
{
    cJSON* keyboard = cJSON_CreateObject();
    cJSON* modes = cJSON_CreateArray();

    if (!keyboard || !modes)
    {
        cJSON_Delete(keyboard);
        cJSON_Delete(modes);
        return NULL;
    }

    for (int mode = 0; mode < KEYMAP_MODES; mode++)
    {
        cJSON* entries = cJSON_CreateArray();

        if (!entries)
            continue;

        for (int key = 0; key < 256; key++)
        {
            cJSON* entry;

            if (!keymap_act[mode][key])
                continue;

            entry = cJSON_CreateObject();
            if (!entry)
                continue;

            cJSON_AddNumberToObject(entry, "key", key);
            cJSON_AddStringToObject(entry, "action", keymap_act[mode][key]);
            cJSON_AddItemToArray(entries, entry);
        }

        cJSON_AddItemToArray(modes, entries);
    }

    cJSON_AddItemToObject(keyboard, "keymaps", modes);
    return keyboard;
}

static const u16b sdl_config_movement_direction_order[] = {
    MOVEMENT_INPUT_DIRECTION_NORTHWEST,
    MOVEMENT_INPUT_DIRECTION_NORTH,
    MOVEMENT_INPUT_DIRECTION_NORTHEAST,
    MOVEMENT_INPUT_DIRECTION_WEST,
    MOVEMENT_INPUT_DIRECTION_EAST,
    MOVEMENT_INPUT_DIRECTION_SOUTHWEST,
    MOVEMENT_INPUT_DIRECTION_SOUTH,
    MOVEMENT_INPUT_DIRECTION_SOUTHEAST
};

static const char* sdl_config_movement_preset_name(u16b preset_id)
{
    switch (preset_id)
    {
    case SDL_MOVEMENT_PRESET_MODERN_ARROWS:
        return "modernArrows";
    case SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC:
        return "modernWasdQezc";
    case SDL_MOVEMENT_PRESET_VI_KEYS:
        return "viKeys";
    case SDL_MOVEMENT_PRESET_CLASSIC_SIL:
        return "classicSil";
    default:
        return "custom";
    }
}

const char* sdl_config_movement_preset_label(u16b preset_id)
{
    switch (preset_id)
    {
    case SDL_MOVEMENT_PRESET_MODERN_ARROWS:
        return "Modern Arrows";
    case SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC:
        return "Modern WASD+QEZC";
    case SDL_MOVEMENT_PRESET_VI_KEYS:
        return "Vi Keys";
    case SDL_MOVEMENT_PRESET_CLASSIC_SIL:
        return "Classic Sil";
    default:
        return "Custom";
    }
}

u16b sdl_config_next_movement_preset(u16b preset_id)
{
    switch (preset_id)
    {
    case SDL_MOVEMENT_PRESET_CLASSIC_SIL:
        return SDL_MOVEMENT_PRESET_MODERN_ARROWS;
    case SDL_MOVEMENT_PRESET_MODERN_ARROWS:
        return SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC;
    case SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC:
        return SDL_MOVEMENT_PRESET_VI_KEYS;
    case SDL_MOVEMENT_PRESET_VI_KEYS:
    default:
        return SDL_MOVEMENT_PRESET_CLASSIC_SIL;
    }
}

static bool sdl_config_movement_preset_from_name(const char* name,
    u16b* out_preset_id)
{
    u16b preset_id = SDL_MOVEMENT_PRESET_NONE;

    if (!name)
        return false;

    if (streq(name, "modernArrows"))
        preset_id = SDL_MOVEMENT_PRESET_MODERN_ARROWS;
    else if (streq(name, "modernWasdQezc"))
        preset_id = SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC;
    else if (streq(name, "viKeys"))
        preset_id = SDL_MOVEMENT_PRESET_VI_KEYS;
    else if (streq(name, "classicSil"))
        preset_id = SDL_MOVEMENT_PRESET_CLASSIC_SIL;
    else if (!streq(name, "custom"))
        return false;

    if (out_preset_id)
        *out_preset_id = preset_id;
    return true;
}

static const char* sdl_config_movement_context_name(u16b context)
{
    switch (context)
    {
    case MOVEMENT_INPUT_CONTEXT_DUNGEON:
        return "dungeon";
    case MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT:
        return "directionPrompt";
    case MOVEMENT_INPUT_CONTEXT_TARGETING:
        return "targeting";
    case MOVEMENT_INPUT_CONTEXT_ANY:
    default:
        return "any";
    }
}

static bool sdl_config_movement_context_from_name(const char* name,
    u16b* out_context)
{
    u16b context = MOVEMENT_INPUT_CONTEXT_ANY;

    if (!name)
        return false;

    if (streq(name, "any"))
        context = MOVEMENT_INPUT_CONTEXT_ANY;
    else if (streq(name, "dungeon"))
        context = MOVEMENT_INPUT_CONTEXT_DUNGEON;
    else if (streq(name, "directionPrompt"))
        context = MOVEMENT_INPUT_CONTEXT_DIRECTION_PROMPT;
    else if (streq(name, "targeting"))
        context = MOVEMENT_INPUT_CONTEXT_TARGETING;
    else
        return false;

    if (out_context)
        *out_context = context;
    return true;
}

static const char* sdl_config_movement_action_name(u16b action)
{
    switch (action)
    {
    case MOVEMENT_INPUT_ACTION_MOVE_DIR:
        return "moveDir";
    case MOVEMENT_INPUT_ACTION_RUN_DIR:
        return "runDir";
    case MOVEMENT_INPUT_ACTION_INTERACT_DIR:
        return "interactDir";
    case MOVEMENT_INPUT_ACTION_WAIT:
        return "wait";
    case MOVEMENT_INPUT_ACTION_REST:
        return "rest";
    default:
        return "none";
    }
}

static bool sdl_config_movement_action_from_name(const char* name,
    u16b* out_action)
{
    u16b action = MOVEMENT_INPUT_ACTION_NONE;

    if (!name)
        return false;

    if (streq(name, "moveDir"))
        action = MOVEMENT_INPUT_ACTION_MOVE_DIR;
    else if (streq(name, "runDir"))
        action = MOVEMENT_INPUT_ACTION_RUN_DIR;
    else if (streq(name, "interactDir"))
        action = MOVEMENT_INPUT_ACTION_INTERACT_DIR;
    else if (streq(name, "wait"))
        action = MOVEMENT_INPUT_ACTION_WAIT;
    else if (streq(name, "rest"))
        action = MOVEMENT_INPUT_ACTION_REST;
    else
        return false;

    if (out_action)
        *out_action = action;
    return true;
}

static const char* sdl_config_movement_direction_name(u16b direction)
{
    switch (direction)
    {
    case MOVEMENT_INPUT_DIRECTION_CENTER:
        return "center";
    case MOVEMENT_INPUT_DIRECTION_NORTH:
        return "north";
    case MOVEMENT_INPUT_DIRECTION_NORTHEAST:
        return "northeast";
    case MOVEMENT_INPUT_DIRECTION_EAST:
        return "east";
    case MOVEMENT_INPUT_DIRECTION_SOUTHEAST:
        return "southeast";
    case MOVEMENT_INPUT_DIRECTION_SOUTH:
        return "south";
    case MOVEMENT_INPUT_DIRECTION_SOUTHWEST:
        return "southwest";
    case MOVEMENT_INPUT_DIRECTION_WEST:
        return "west";
    case MOVEMENT_INPUT_DIRECTION_NORTHWEST:
        return "northwest";
    default:
        return "none";
    }
}

static bool sdl_config_movement_direction_from_name(const char* name,
    u16b* out_direction)
{
    u16b direction = MOVEMENT_INPUT_DIRECTION_NONE;

    if (!name)
        return false;

    if (streq(name, "none"))
        direction = MOVEMENT_INPUT_DIRECTION_NONE;
    else if (streq(name, "center"))
        direction = MOVEMENT_INPUT_DIRECTION_CENTER;
    else if (streq(name, "north"))
        direction = MOVEMENT_INPUT_DIRECTION_NORTH;
    else if (streq(name, "northeast"))
        direction = MOVEMENT_INPUT_DIRECTION_NORTHEAST;
    else if (streq(name, "east"))
        direction = MOVEMENT_INPUT_DIRECTION_EAST;
    else if (streq(name, "southeast"))
        direction = MOVEMENT_INPUT_DIRECTION_SOUTHEAST;
    else if (streq(name, "south"))
        direction = MOVEMENT_INPUT_DIRECTION_SOUTH;
    else if (streq(name, "southwest"))
        direction = MOVEMENT_INPUT_DIRECTION_SOUTHWEST;
    else if (streq(name, "west"))
        direction = MOVEMENT_INPUT_DIRECTION_WEST;
    else if (streq(name, "northwest"))
        direction = MOVEMENT_INPUT_DIRECTION_NORTHWEST;
    else
        return false;

    if (out_direction)
        *out_direction = direction;
    return true;
}

static bool sdl_config_movement_binding_equals(
    const movement_input_binding* left, const movement_input_binding* right)
{
    if (!left || !right)
        return false;

    return left->context == right->context
        && left->action == right->action
        && left->direction == right->direction
        && left->required_modifiers == right->required_modifiers
        && left->forbidden_modifiers == right->forbidden_modifiers
        && left->trigger == right->trigger
        && left->trigger_aux == right->trigger_aux;
}

bool sdl_config_append_movement_binding(struct sdl_config* cfg,
    const movement_input_binding* binding)
{
    if (!cfg || !binding || !movement_input_binding_is_valid(binding))
        return false;
    if (cfg->movement_binding_count >= SDL_MOVEMENT_BINDING_MAX)
        return false;

    for (u16b i = 0; i < cfg->movement_binding_count; i++)
    {
        if (sdl_config_movement_binding_equals(
                &cfg->movement_bindings[i], binding))
        {
            return true;
        }
    }

    cfg->movement_bindings[cfg->movement_binding_count++] = *binding;
    return true;
}

static void sdl_config_init_keyboard_movement_binding(
    movement_input_binding* binding, u16b action, u16b direction,
    SDL_Scancode scancode, u16b required_modifiers, u16b forbidden_modifiers)
{
    movement_input_binding_clear(binding);
    binding->context = MOVEMENT_INPUT_CONTEXT_ANY;
    binding->action = action;
    binding->direction = direction;
    binding->required_modifiers = required_modifiers;
    binding->forbidden_modifiers = forbidden_modifiers;
    binding->trigger = (u32b)scancode;
}

static bool sdl_config_add_keyboard_movement_binding(struct sdl_config* cfg,
    u16b action, u16b direction, SDL_Scancode scancode, u16b required_modifiers,
    u16b forbidden_modifiers)
{
    movement_input_binding binding;

    if (!cfg || scancode == SDL_SCANCODE_UNKNOWN)
        return false;

    sdl_config_init_keyboard_movement_binding(&binding, action, direction,
        scancode, required_modifiers, forbidden_modifiers);
    return sdl_config_append_movement_binding(cfg, &binding);
}

static void sdl_config_add_directional_movement_preset_set(
    struct sdl_config* cfg, const SDL_Scancode* primary_scancodes)
{
    static const SDL_Scancode keypad_scancodes[] = {
        SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9,
        SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6,
        SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3
    };
    const u16b plain_forbidden = MOVEMENT_INPUT_MODIFIER_SHIFT
        | MOVEMENT_INPUT_MODIFIER_CTRL | MOVEMENT_INPUT_MODIFIER_ALT
        | MOVEMENT_INPUT_MODIFIER_META;
    const u16b shift_forbidden = MOVEMENT_INPUT_MODIFIER_CTRL
        | MOVEMENT_INPUT_MODIFIER_ALT | MOVEMENT_INPUT_MODIFIER_META;
    const u16b ctrl_forbidden = MOVEMENT_INPUT_MODIFIER_SHIFT
        | MOVEMENT_INPUT_MODIFIER_ALT | MOVEMENT_INPUT_MODIFIER_META;

    for (size_t i = 0; i < N_ELEMENTS(sdl_config_movement_direction_order); i++)
    {
        u16b direction = sdl_config_movement_direction_order[i];

        (void)sdl_config_add_keyboard_movement_binding(cfg,
            MOVEMENT_INPUT_ACTION_MOVE_DIR, direction, primary_scancodes[i],
            0, plain_forbidden);
        (void)sdl_config_add_keyboard_movement_binding(cfg,
            MOVEMENT_INPUT_ACTION_MOVE_DIR, direction, keypad_scancodes[i],
            0, plain_forbidden);
        (void)sdl_config_add_keyboard_movement_binding(cfg,
            MOVEMENT_INPUT_ACTION_RUN_DIR, direction, primary_scancodes[i],
            MOVEMENT_INPUT_MODIFIER_SHIFT, shift_forbidden);
        (void)sdl_config_add_keyboard_movement_binding(cfg,
            MOVEMENT_INPUT_ACTION_RUN_DIR, direction, keypad_scancodes[i],
            MOVEMENT_INPUT_MODIFIER_SHIFT, shift_forbidden);
        (void)sdl_config_add_keyboard_movement_binding(cfg,
            MOVEMENT_INPUT_ACTION_INTERACT_DIR, direction, primary_scancodes[i],
            MOVEMENT_INPUT_MODIFIER_CTRL, ctrl_forbidden);
        (void)sdl_config_add_keyboard_movement_binding(cfg,
            MOVEMENT_INPUT_ACTION_INTERACT_DIR, direction, keypad_scancodes[i],
            MOVEMENT_INPUT_MODIFIER_CTRL, ctrl_forbidden);
    }
}

static void sdl_config_add_wait_rest_movement_bindings(struct sdl_config* cfg,
    SDL_Scancode wait_primary, SDL_Scancode rest_primary,
    SDL_Scancode wait_secondary, SDL_Scancode rest_secondary)
{
    const u16b plain_forbidden = MOVEMENT_INPUT_MODIFIER_SHIFT
        | MOVEMENT_INPUT_MODIFIER_CTRL | MOVEMENT_INPUT_MODIFIER_ALT
        | MOVEMENT_INPUT_MODIFIER_META;
    const u16b shift_forbidden = MOVEMENT_INPUT_MODIFIER_CTRL
        | MOVEMENT_INPUT_MODIFIER_ALT | MOVEMENT_INPUT_MODIFIER_META;

    (void)sdl_config_add_keyboard_movement_binding(cfg,
        MOVEMENT_INPUT_ACTION_WAIT, MOVEMENT_INPUT_DIRECTION_NONE,
        wait_primary, 0, plain_forbidden);
    (void)sdl_config_add_keyboard_movement_binding(cfg,
        MOVEMENT_INPUT_ACTION_WAIT, MOVEMENT_INPUT_DIRECTION_NONE,
        wait_secondary, 0, plain_forbidden);
    (void)sdl_config_add_keyboard_movement_binding(cfg,
        MOVEMENT_INPUT_ACTION_REST, MOVEMENT_INPUT_DIRECTION_NONE,
        rest_primary, MOVEMENT_INPUT_MODIFIER_SHIFT, shift_forbidden);
    (void)sdl_config_add_keyboard_movement_binding(cfg,
        MOVEMENT_INPUT_ACTION_REST, MOVEMENT_INPUT_DIRECTION_NONE,
        rest_secondary, MOVEMENT_INPUT_MODIFIER_SHIFT, shift_forbidden);
}

void sdl_config_clear_movement_bindings(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    cfg->movement_keyboard_present = false;
    cfg->movement_keyboard_preset = SDL_MOVEMENT_PRESET_NONE;
    cfg->movement_binding_count = 0;
    memset(cfg->movement_bindings, 0, sizeof(cfg->movement_bindings));
}

void sdl_config_set_default_movement_bindings(struct sdl_config* cfg,
    u16b preset_id)
{
    static const SDL_Scancode classic_scancodes[] = {
        SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9,
        SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6,
        SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3
    };
    static const SDL_Scancode arrows_scancodes[] = {
        SDL_SCANCODE_HOME, SDL_SCANCODE_UP, SDL_SCANCODE_PAGEUP,
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_END, SDL_SCANCODE_DOWN, SDL_SCANCODE_PAGEDOWN
    };
    /* Modern Arrows: real arrows for orthogonals, the punctuation block near
     * the arrows ( ; ' . / ) for diagonals. These take run (Shift) / interact
     * (Ctrl) like any movement key, so Shift+. / Shift+/ etc. become run rather
     * than '>' / '?' here; players who want those should use Classic Sil. */
    static const SDL_Scancode modern_arrows_scancodes[] = {
        SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_UP, SDL_SCANCODE_APOSTROPHE,
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_PERIOD, SDL_SCANCODE_DOWN, SDL_SCANCODE_SLASH
    };
    static const SDL_Scancode wasd_scancodes[] = {
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E,
        SDL_SCANCODE_A, SDL_SCANCODE_D,
        SDL_SCANCODE_Z, SDL_SCANCODE_S, SDL_SCANCODE_C
    };
    static const SDL_Scancode vi_scancodes[] = {
        SDL_SCANCODE_Y, SDL_SCANCODE_K, SDL_SCANCODE_U,
        SDL_SCANCODE_H, SDL_SCANCODE_L,
        SDL_SCANCODE_B, SDL_SCANCODE_J, SDL_SCANCODE_N
    };
    const SDL_Scancode* directional_scancodes = classic_scancodes;

    if (!cfg)
        return;

    switch (preset_id)
    {
    case SDL_MOVEMENT_PRESET_MODERN_ARROWS:
        directional_scancodes = modern_arrows_scancodes;
        break;
    case SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC:
        directional_scancodes = wasd_scancodes;
        break;
    case SDL_MOVEMENT_PRESET_VI_KEYS:
        directional_scancodes = vi_scancodes;
        break;
    case SDL_MOVEMENT_PRESET_CLASSIC_SIL:
    default:
        directional_scancodes = classic_scancodes;
        preset_id = SDL_MOVEMENT_PRESET_CLASSIC_SIL;
        break;
    }

    sdl_config_clear_movement_bindings(cfg);
    cfg->movement_keyboard_present = true;
    cfg->movement_keyboard_preset = preset_id;

    sdl_config_add_directional_movement_preset_set(cfg, directional_scancodes);

    if (preset_id == SDL_MOVEMENT_PRESET_MODERN_ARROWS)
    {
        /* '.' / '/' are diagonals here, so wait/rest live on the numpad centre
         * rather than the usual PERIOD. */
        sdl_config_add_wait_rest_movement_bindings(cfg, SDL_SCANCODE_KP_5,
            SDL_SCANCODE_KP_5, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN);
    }
    else if (preset_id == SDL_MOVEMENT_PRESET_CLASSIC_SIL)
    {
        sdl_config_add_directional_movement_preset_set(cfg, arrows_scancodes);
        sdl_config_add_wait_rest_movement_bindings(cfg, SDL_SCANCODE_KP_5,
            SDL_SCANCODE_KP_5, SDL_SCANCODE_Z, SDL_SCANCODE_Z);
    }
    else
    {
        sdl_config_add_wait_rest_movement_bindings(cfg, SDL_SCANCODE_PERIOD,
            SDL_SCANCODE_PERIOD, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_5);
    }
}

bool sdl_config_set_movement_binding(struct sdl_config* cfg, u16b action,
    u16b direction, const movement_input_binding* binding)
{
    u16b out = 0;

    if (!cfg)
        return false;

    while (out < cfg->movement_binding_count)
    {
        movement_input_binding* existing = &cfg->movement_bindings[out];

        if (existing->action == action
            && (!movement_input_action_is_directional(action)
                || existing->direction == direction))
        {
            memmove(existing, existing + 1,
                (cfg->movement_binding_count - out - 1)
                    * sizeof(cfg->movement_bindings[0]));
            cfg->movement_binding_count--;
            continue;
        }
        out++;
    }

    if (!binding)
    {
        cfg->movement_keyboard_present = true;
        cfg->movement_keyboard_preset = SDL_MOVEMENT_PRESET_NONE;
        return true;
    }

    cfg->movement_keyboard_present = true;
    cfg->movement_keyboard_preset = SDL_MOVEMENT_PRESET_NONE;
    return sdl_config_append_movement_binding(cfg, binding);
}

bool sdl_config_resolve_movement_binding(const struct sdl_config* cfg,
    u16b context, u32b trigger, u32b trigger_aux, u16b modifiers,
    movement_input_command* out_command)
{
    const movement_input_binding* selected = NULL;
    int best_score = -1;

    if (!cfg || !out_command || context == MOVEMENT_INPUT_CONTEXT_NONE)
        return false;

    movement_input_command_clear(out_command);

    for (u16b i = 0; i < cfg->movement_binding_count; i++)
    {
        const movement_input_binding* binding = &cfg->movement_bindings[i];
        int score = 0;

        if (!movement_input_binding_matches(binding, context, trigger,
                trigger_aux, modifiers))
        {
            continue;
        }

        if (binding->context == context)
            score += 4;
        if (binding->trigger_aux != 0)
            score += 1;

        if (score > best_score)
        {
            best_score = score;
            selected = binding;
        }
    }

    if (!selected)
        return false;

    return movement_input_command_from_binding(selected, context, out_command);
}

bool sdl_config_scancode_is_plain_move_letter(const struct sdl_config* cfg,
    u32b scancode)
{
    if (!cfg)
        return false;
    if (scancode < (u32b)SDL_SCANCODE_A || scancode > (u32b)SDL_SCANCODE_Z)
        return false;

    for (u16b i = 0; i < cfg->movement_binding_count; i++)
    {
        const movement_input_binding* binding = &cfg->movement_bindings[i];

        if (!movement_input_binding_is_valid(binding))
            continue;
        if (binding->action != MOVEMENT_INPUT_ACTION_MOVE_DIR)
            continue;
        if (binding->required_modifiers)
            continue;
        if (binding->trigger == scancode)
            return true;
    }

    return false;
}

static void sdl_config_load_movement_bindings(cJSON* root,
    struct sdl_config* cfg)
{
    cJSON* movement;
    cJSON* preset;
    cJSON* bindings;
    u16b preset_id = SDL_MOVEMENT_PRESET_CLASSIC_SIL;
    bool valid_preset = true;

    if (!root || !cfg)
        return;

    movement = cJSON_GetObjectItemCaseSensitive(root, "movement");
    if (!movement)
        return;

    if (!cJSON_IsObject(movement))
    {
        log_warn("movement config is malformed; using Classic Sil movement defaults");
        sdl_config_set_default_movement_bindings(cfg,
            SDL_MOVEMENT_PRESET_CLASSIC_SIL);
        return;
    }

    preset = cJSON_GetObjectItemCaseSensitive(movement, "keyboardPreset");
    if (cJSON_IsString(preset) && preset->valuestring)
    {
        valid_preset = sdl_config_movement_preset_from_name(
            preset->valuestring, &preset_id);
    }

    if (!valid_preset)
    {
        log_warn("movement.keyboardPreset is invalid; using Classic Sil movement defaults");
        sdl_config_set_default_movement_bindings(cfg,
            SDL_MOVEMENT_PRESET_CLASSIC_SIL);
        return;
    }

    bindings = cJSON_GetObjectItemCaseSensitive(movement, "keyboardBindings");
    if (!bindings)
    {
        if (preset_id == SDL_MOVEMENT_PRESET_NONE)
        {
            log_warn("custom movement config has no keyboardBindings; using Classic Sil movement defaults");
            sdl_config_set_default_movement_bindings(cfg,
                SDL_MOVEMENT_PRESET_CLASSIC_SIL);
        }
        else
        {
            sdl_config_set_default_movement_bindings(cfg, preset_id);
        }
        return;
    }

    if (!cJSON_IsArray(bindings))
    {
        log_warn("movement.keyboardBindings is malformed; using Classic Sil movement defaults");
        sdl_config_set_default_movement_bindings(cfg,
            SDL_MOVEMENT_PRESET_CLASSIC_SIL);
        return;
    }

    sdl_config_clear_movement_bindings(cfg);
    cfg->movement_keyboard_present = true;
    cfg->movement_keyboard_preset = preset_id;

    for (int i = 0; i < cJSON_GetArraySize(bindings); i++)
    {
        cJSON* item = cJSON_GetArrayItem(bindings, i);
        cJSON* context_item;
        cJSON* action_item;
        cJSON* direction_item;
        cJSON* trigger_item;
        cJSON* trigger_aux_item;
        cJSON* required_item;
        cJSON* forbidden_item;
        movement_input_binding binding;

        if (!cJSON_IsObject(item))
            continue;

        movement_input_binding_clear(&binding);

        context_item = cJSON_GetObjectItemCaseSensitive(item, "context");
        action_item = cJSON_GetObjectItemCaseSensitive(item, "action");
        direction_item = cJSON_GetObjectItemCaseSensitive(item, "direction");
        trigger_item = cJSON_GetObjectItemCaseSensitive(item, "trigger");
        trigger_aux_item = cJSON_GetObjectItemCaseSensitive(item, "triggerAux");
        required_item = cJSON_GetObjectItemCaseSensitive(item,
            "requiredModifiers");
        forbidden_item = cJSON_GetObjectItemCaseSensitive(item,
            "forbiddenModifiers");

        if (!cJSON_IsString(context_item) || !context_item->valuestring
            || !sdl_config_movement_context_from_name(context_item->valuestring,
                &binding.context))
        {
            continue;
        }
        if (!cJSON_IsString(action_item) || !action_item->valuestring
            || !sdl_config_movement_action_from_name(action_item->valuestring,
                &binding.action))
        {
            continue;
        }
        if (cJSON_IsString(direction_item) && direction_item->valuestring)
        {
            if (!sdl_config_movement_direction_from_name(
                    direction_item->valuestring, &binding.direction))
            {
                continue;
            }
        }
        if (!cJSON_IsNumber(trigger_item) || trigger_item->valueint <= 0)
            continue;

        binding.trigger = (u32b)trigger_item->valueint;
        if (cJSON_IsNumber(trigger_aux_item) && trigger_aux_item->valueint > 0)
            binding.trigger_aux = (u32b)trigger_aux_item->valueint;
        if (cJSON_IsNumber(required_item) && required_item->valueint >= 0)
            binding.required_modifiers = (u16b)required_item->valueint;
        if (cJSON_IsNumber(forbidden_item) && forbidden_item->valueint >= 0)
            binding.forbidden_modifiers = (u16b)forbidden_item->valueint;

        if (!sdl_config_append_movement_binding(cfg, &binding))
        {
            log_warn("Skipping invalid or duplicate movement binding at JSON index %d",
                i);
        }
    }

    if (cfg->movement_binding_count == 0)
    {
        log_warn("movement.keyboardBindings has no valid bindings; using Classic Sil movement defaults");
        sdl_config_set_default_movement_bindings(cfg,
            SDL_MOVEMENT_PRESET_CLASSIC_SIL);
    }
}

static void sdl_config_save_movement_bindings(cJSON* root,
    const struct sdl_config* cfg)
{
    cJSON* movement;
    cJSON* bindings;

    if (!root || !cfg)
        return;

    movement = cJSON_CreateObject();
    if (!movement)
        return;

    cJSON_AddNumberToObject(movement, "version", MOVEMENT_INPUT_FORMAT_VERSION);
    cJSON_AddStringToObject(movement, "keyboardPreset",
        sdl_config_movement_preset_name(cfg->movement_keyboard_preset));

    bindings = cJSON_CreateArray();
    if (bindings)
    {
        for (u16b i = 0; i < cfg->movement_binding_count; i++)
        {
            const movement_input_binding* binding = &cfg->movement_bindings[i];
            cJSON* item;

            if (!movement_input_binding_is_valid(binding))
                continue;

            item = cJSON_CreateObject();
            if (!item)
                continue;

            cJSON_AddStringToObject(item, "context",
                sdl_config_movement_context_name(binding->context));
            cJSON_AddStringToObject(item, "action",
                sdl_config_movement_action_name(binding->action));
            cJSON_AddStringToObject(item, "direction",
                sdl_config_movement_direction_name(binding->direction));
            cJSON_AddNumberToObject(item, "trigger", (double)binding->trigger);
            if (binding->trigger_aux != 0)
            {
                cJSON_AddNumberToObject(item, "triggerAux",
                    (double)binding->trigger_aux);
            }
            cJSON_AddNumberToObject(item, "requiredModifiers",
                (double)binding->required_modifiers);
            cJSON_AddNumberToObject(item, "forbiddenModifiers",
                (double)binding->forbidden_modifiers);
            cJSON_AddItemToArray(bindings, item);
        }

        cJSON_AddItemToObject(movement, "keyboardBindings", bindings);
    }

    cJSON_AddItemToObject(root, "movement", movement);
}

/*
 * Legacy resolution preset data.
 *
 * First-run defaults are now computed dynamically from the display size and
 * the platform minimum terminal size, so no resolution preset table is used.
 */
#if 0
// Resolution-specific default configuration profile
// Only includes values that differ per resolution
struct resolution_profile {
    int width;
    int height;
    const char* name;
    
    // Resolution-specific SDL settings
    int main_view_scale;
    int aux_view_font_size;
    
    // Pane configurations (up to 8 panes)
    int pane_count;
    struct {
        enum pane_type type;
        enum pane_placement where;
        int rows;
        int cols;
    } panes[MAX_PANE_CONFIGS];
};

// Resolution profiles database - add new resolutions here!
// 
// Only resolution-specific values are stored here.
// Common defaults (margin=4, fullscreen=true, tiles=true) are set in sdl_config_set_defaults()
// 
// LAYOUT CALCULATION LOGIC:
// 1. Normal minimum main terminal: 80x24 cells
// 2. Try maximum scale (up to 4) that fits: scale 4 = 2560x1536, scale 3 = 1920x1152, scale 2 = 1280x768, scale 1 = 640x384
// 3. Aux view font size: auto-derived from scale at 2/3 main size
//    (left panel uses 3/4 main size)
// 4. Right pane: if we can fit >=40 columns (using aux_font_size / 2 char width), add right pane
//    - Right pane contains: Inventory (22 rows), Worn (17 rows), Info (remaining, rows=0 means auto)
//    - Right pane width: 40-50 columns depending on available space
// 5. Bottom pane: add one combined Log pane (messages + combat), defaulting to 4 rows
// 6. Main terminal expands to use all remaining space
//
// To add a new resolution:
// 1. Copy an existing profile block
// 2. Update width, height, name
// 3. Adjust main_view_scale, aux_view_font_size, and pane layout following the logic above
// 4. That's it! The function will automatically pick it up.
//
static const struct resolution_profile resolution_profiles[] = {
    // 800x600 (SVGA)
    { .width = 800, .height = 600, .name = "800x600 (SVGA)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1024x768 (XGA)
    { .width = 1024, .height = 768, .name = "1024x768 (XGA)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 40 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1152x864
    { .width = 1152, .height = 864, .name = "1152x864", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1280x720 (HD 720p)
    { .width = 1280, .height = 720, .name = "1280x720 (HD 720p)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1280x768
    { .width = 1280, .height = 768, .name = "1280x768", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    // 1280x800 (WXGA)
    { .width = 1280, .height = 800, .name = "1280x800 (WXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 1, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1280x960
    { .width = 1280, .height = 960, .name = "1280x960", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1280x1024 (SXGA)
    { .width = 1280, .height = 1024, .name = "1280x1024 (SXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1360x768
    { .width = 1360, .height = 768, .name = "1360x768", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    // 1366x768 (HD)
    { .width = 1366, .height = 768, .name = "1366x768 (HD)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    // 1400x1050
    { .width = 1400, .height = 1050, .name = "1400x1050", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1440x900
    { .width = 1440, .height = 900, .name = "1440x900", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1536x864
    { .width = 1536, .height = 864, .name = "1536x864", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 3, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1600x900 (HD+)
    { .width = 1600, .height = 900, .name = "1600x900 (HD+)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1600x1200 (UXGA)
    { .width = 1600, .height = 1200, .name = "1600x1200 (UXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1680x1050
    { .width = 1680, .height = 1050, .name = "1680x1050", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1920x1080 (Full HD)
    { .width = 1920, .height = 1080, .name = "1920x1080 (Full HD)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 1920x1200 (WUXGA)
    { .width = 1920, .height = 1200, .name = "1920x1200 (WUXGA)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 1, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2048x1152
    { .width = 2048, .height = 1152, .name = "2048x1152", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 0, .panes = {} },
    
    // 2256x1504 (Surface Laptop 13.5")
    { .width = 2256, .height = 1504, .name = "2256x1504 (Surface Laptop 13.5\")", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2160x1440
    { .width = 2160, .height = 1440, .name = "2160x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2304x1440 (LG UltraFine scaled)
    { .width = 2304, .height = 1440, .name = "2304x1440 (LG UltraFine scaled)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2520x1680 (MacBook Air 13" M2/M3)
    { .width = 2520, .height = 1680, .name = "2520x1680 (MacBook Air 13\" M2/M3)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1080 (Ultrawide)
    { .width = 2560, .height = 1080, .name = "2560x1080 (Ultrawide)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1440 (QHD)
    { .width = 2560, .height = 1440, .name = "2560x1440 (QHD)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1600 (MacBook 13")
    { .width = 2560, .height = 1600, .name = "2560x1600 (MacBook 13\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1700 (Dell XPS 17")
    { .width = 2560, .height = 1700, .name = "2560x1700 (Dell XPS 17\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2736x1824 (Surface Book)
    { .width = 2736, .height = 1824, .name = "2736x1824 (Surface Book)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 45 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1620
    { .width = 2880, .height = 1620, .name = "2880x1620", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1800 (MacBook 15")
    { .width = 2880, .height = 1800, .name = "2880x1800 (MacBook 15\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1920 (Surface Laptop 15")
    { .width = 2880, .height = 1920, .name = "2880x1920 (Surface Laptop 15\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3000x2000 (Surface Laptop)
    { .width = 3000, .height = 2000, .name = "3000x2000 (Surface Laptop)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3024x1964 (MacBook Pro 14" base)
    { .width = 3024, .height = 1964, .name = "3024x1964 (MacBook Pro 14\" base)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3072x1920 (MacBook Pro 16")
    { .width = 3072, .height = 1920, .name = "3072x1920 (MacBook Pro 16\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3200x1800
    { .width = 3200, .height = 1800, .name = "3200x1800", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3240x2160
    { .width = 3240, .height = 2160, .name = "3240x2160", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3440x1440 (Ultrawide QHD)
    { .width = 3440, .height = 1440, .name = "3440x1440 (Ultrawide QHD)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3456x2234 (MacBook Pro 14")
    { .width = 3456, .height = 2234, .name = "3456x2234 (MacBook Pro 14\")", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1080 (Super Ultrawide)
    { .width = 3840, .height = 1080, .name = "3840x1080 (Super Ultrawide)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1200
    { .width = 3840, .height = 1200, .name = "3840x1200", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 1, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1440
    { .width = 3840, .height = 1440, .name = "3840x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1600
    { .width = 3840, .height = 1600, .name = "3840x1600", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x2160 (4K UHD)
    { .width = 3840, .height = 2160, .name = "3840x2160 (4K UHD)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x2400 (Dell UltraSharp)
    { .width = 3840, .height = 2400, .name = "3840x2400 (Dell UltraSharp)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 4096x2160 (DCI 4K)
    { .width = 4096, .height = 2160, .name = "4096x2160 (DCI 4K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 4480x1440
    { .width = 4480, .height = 1440, .name = "4480x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x1440 (Super Ultrawide)
    { .width = 5120, .height = 1440, .name = "5120x1440 (Super Ultrawide)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x2160 (5K Ultrawide)
    { .width = 5120, .height = 2160, .name = "5120x2160 (5K Ultrawide)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x2880 (5K)
    { .width = 5120, .height = 2880, .name = "5120x2880 (5K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 6016x3384 (6K)
    { .width = 6016, .height = 3384, .name = "6016x3384 (6K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 7680x4320 (8K UHD)
    { .width = 7680, .height = 4320, .name = "7680x4320 (8K UHD)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } }
};

#define NUM_RESOLUTION_PROFILES (sizeof(resolution_profiles) / sizeof(resolution_profiles[0]))
#endif

enum {
    SDL_CONFIG_DEFAULT_LOG_PANE_ROWS = 5,
};

static const char* pane_type_to_string(enum pane_type type)
{
    switch (type) {
        case PANE_MAIN: return "MAIN";
        case PANE_INVENTORY: return "INVENTORY";
        case PANE_SUPPLY: return "SUPPLY";
        case PANE_WORN: return "WORN";
        case PANE_ROLLS: return "ROLLS";
        case PANE_INFO: return "INFO";
        case PANE_CHARACTER: return "CHARACTER";
        case PANE_LOG: return "LOG";
        case PANE_MONSTERS: return "MONSTERS";
        case PANE_MAP: return "MAP";
        case PANE_TOUCH: return "TOUCH";
        case PANE_LEFT_PANEL: return "LEFT_PANEL";
        case PANE_STATUS: return "STATUS";
        case PANE_DEPTH: return "DEPTH";
        case PANE_DESCRIPTION: return "DESCRIPTION";
        case PANE_OVERLAY_MENU: return "OVERLAY_MENU";
        case PANE_COMBAT: return "COMBAT";
        default: return "MAIN";
    }
}

static enum pane_type parse_pane_type(const char* value)
{
    if (!value)
        return PANE_MAIN;
    if (strcmp(value, "MAIN") == 0) return PANE_MAIN;
    if (strcmp(value, "INVENTORY") == 0) return PANE_INVENTORY;
    if (strcmp(value, "SUPPLY") == 0 || strcmp(value, "SUPPLIES") == 0)
        return PANE_SUPPLY;
    if (strcmp(value, "WORN") == 0) return PANE_WORN;
    if (strcmp(value, "ROLLS") == 0) return PANE_ROLLS;
    if (strcmp(value, "INFO") == 0) return PANE_INFO;
    if (strcmp(value, "CHARACTER") == 0) return PANE_CHARACTER;
    if (strcmp(value, "LOG") == 0) return PANE_LOG;
    if (strcmp(value, "MONSTERS") == 0) return PANE_MONSTERS;
    if (strcmp(value, "MAP") == 0) return PANE_MAP;
    if (strcmp(value, "TOUCH") == 0) return PANE_TOUCH;
    if (strcmp(value, "LEFT_PANEL") == 0 || strcmp(value, "LEFT PANEL") == 0)
        return PANE_LEFT_PANEL;
    if (strcmp(value, "STATUS") == 0) return PANE_STATUS;
    if (strcmp(value, "DEPTH") == 0 || strcmp(value, "DEPTH_PANE") == 0
        || strcmp(value, "DEPTH PANE") == 0)
        return PANE_DEPTH;
    if (strcmp(value, "MAIN_MENU") == 0 || strcmp(value, "MAIN MENU") == 0)
        return PANE_MAIN_MENU;
    if (strcmp(value, "DESCRIPTION") == 0
        || strcmp(value, "DESCRIPTION_OVERLAY") == 0
        || strcmp(value, "DESCRIPTION OVERLAY") == 0)
        return PANE_DESCRIPTION;
    if (strcmp(value, "OVERLAY_MENU") == 0
        || strcmp(value, "OVERLAY MENU") == 0
        || strcmp(value, "TOP_PANEL") == 0
        || strcmp(value, "TOP PANEL") == 0)
        return PANE_OVERLAY_MENU;
    if (strcmp(value, "COMBAT") == 0
        || strcmp(value, "COMBAT_OVERLAY") == 0
        || strcmp(value, "COMBAT OVERLAY") == 0)
        return PANE_COMBAT;
    return PANE_MAIN;
}

static const char* pane_placement_to_string(enum pane_placement where)
{
    return pane_placement_name(where);
}

static enum pane_placement parse_pane_placement(const char* value)
{
    if (!value)
        return PLACE_RIGHT;
    if (strcmp(value, "BOTTOM") == 0) return PLACE_BOTTOM;
    if (strcmp(value, "DOUBLE_BOTTOM") == 0 || strcmp(value, "DOUBLE BOTTOM") == 0)
        return PLACE_DOUBLE_BOTTOM;
    if (strcmp(value, "RIGHT") == 0) return PLACE_RIGHT;
    if (strcmp(value, "LEFT") == 0) return PLACE_LEFT;
    if (strcmp(value, "TOP_LEFT") == 0 || strcmp(value, "TOP LEFT") == 0)
        return PLACE_TOP_LEFT;
    if (strcmp(value, "TOP_RIGHT") == 0 || strcmp(value, "TOP RIGHT") == 0)
        return PLACE_TOP_RIGHT;
    if (strcmp(value, "TOP_CENTER") == 0 || strcmp(value, "TOP CENTER") == 0)
        return PLACE_TOP_CENTER;
    if (strcmp(value, "BOTTOM_LEFT") == 0 || strcmp(value, "BOTTOM LEFT") == 0)
        return PLACE_BOTTOM_LEFT;
    if (strcmp(value, "BOTTOM_RIGHT") == 0 || strcmp(value, "BOTTOM RIGHT") == 0)
        return PLACE_BOTTOM_RIGHT;
    if (strcmp(value, "BOTTOM_CENTER") == 0
        || strcmp(value, "BOTTOM CENTER") == 0)
        return PLACE_BOTTOM_CENTER;
    if (strcmp(value, "LEFT_CENTER") == 0 || strcmp(value, "LEFT CENTER") == 0)
        return PLACE_LEFT_CENTER;
    if (strcmp(value, "RIGHT_CENTER") == 0
        || strcmp(value, "RIGHT CENTER") == 0)
        return PLACE_RIGHT_CENTER;
    if (strcmp(value, "DOUBLE_LEFT") == 0 || strcmp(value, "DOUBLE LEFT") == 0)
        return PLACE_DOUBLE_LEFT;
    if (strcmp(value, "DOUBLE_RIGHT") == 0 || strcmp(value, "DOUBLE RIGHT") == 0)
        return PLACE_DOUBLE_RIGHT;
    return PLACE_RIGHT;
}

static const char* min_terminal_mode_to_string(int mode)
{
    switch (mode) {
        case SDL_MIN_TERMINAL_COMPACT: return "COMPACT";
        case SDL_MIN_TERMINAL_NORMAL: return "NORMAL";
        default: return "NORMAL";
    }
}

static int parse_min_terminal_mode(const char* value)
{
    if (!value)
        return SDL_MIN_TERMINAL_NORMAL;
    if (strcmp(value, "COMPACT") == 0) return SDL_MIN_TERMINAL_COMPACT;
    if (strcmp(value, "NORMAL") == 0) return SDL_MIN_TERMINAL_NORMAL;
    return SDL_MIN_TERMINAL_NORMAL;
}

static const char* left_panel_compact_mode_to_string(int mode)
{
    switch (mode) {
        case SDL_LEFT_PANEL_COMPACT_ROW: return "ROW";
        case SDL_LEFT_PANEL_COMPACT_COLUMN: return "COLUMN";
        default: return "COLUMN";
    }
}

static int parse_left_panel_compact_mode(const char* value)
{
    if (!value)
        return SDL_LEFT_PANEL_COMPACT_COLUMN;
    if (strcmp(value, "ROW") == 0) return SDL_LEFT_PANEL_COMPACT_ROW;
    if (strcmp(value, "COLUMN") == 0) return SDL_LEFT_PANEL_COMPACT_COLUMN;
    return SDL_LEFT_PANEL_COMPACT_COLUMN;
}

static const char* touch_profile_to_string(int profile)
{
    switch (profile) {
        case SDL_TOUCH_PROFILE_CORNERS: return "CORNERS";
        case SDL_TOUCH_PROFILE_ROUND_WHEEL: return "ROUND_WHEEL";
        case SDL_TOUCH_PROFILE_TOUCH_PANE:
        default:
            return "TOUCH_PANE";
    }
}

static int normalize_touch_profile(int profile)
{
    if (profile >= SDL_TOUCH_PROFILE_TOUCH_PANE
        && profile < SDL_TOUCH_PROFILE_COUNT)
    {
        return profile;
    }

    return SDL_TOUCH_PROFILE_TOUCH_PANE;
}

static int parse_touch_profile(const char* value)
{
    if (!value)
        return SDL_TOUCH_PROFILE_TOUCH_PANE;
    if (strcmp(value, "TOUCH_PANE") == 0) return SDL_TOUCH_PROFILE_TOUCH_PANE;
    if (strcmp(value, "TOUCH_PANEL") == 0) return SDL_TOUCH_PROFILE_TOUCH_PANE;
    if (strcmp(value, "CORNERS") == 0) return SDL_TOUCH_PROFILE_CORNERS;
    if (strcmp(value, "ROUND_WHEEL") == 0) return SDL_TOUCH_PROFILE_ROUND_WHEEL;
    if (strcmp(value, "ROUND") == 0) return SDL_TOUCH_PROFILE_ROUND_WHEEL;
    return SDL_TOUCH_PROFILE_TOUCH_PANE;
}

static const char* touch_top_panel_mode_to_string(int mode)
{
    switch (mode) {
        case SDL_TOUCH_TOP_PANEL_MODE_LONG: return "LONG";
        case SDL_TOUCH_TOP_PANEL_MODE_SHORT:
        default:
            return "SHORT";
    }
}

static int normalize_touch_top_panel_mode(int mode)
{
    if (mode == SDL_TOUCH_TOP_PANEL_MODE_SHORT
        || mode == SDL_TOUCH_TOP_PANEL_MODE_LONG)
    {
        return mode;
    }

    return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
}

static int normalize_touch_top_panel_button_count(int count)
{
    if (count < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT_MIN)
        return SDL_TOUCH_TOP_PANEL_BUTTON_COUNT_MIN;
    if (count > SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return SDL_TOUCH_TOP_PANEL_BUTTON_COUNT;
    return count;
}

static int parse_touch_top_panel_mode(const char* value)
{
    if (!value)
        return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
    if (strcmp(value, "LONG") == 0) return SDL_TOUCH_TOP_PANEL_MODE_LONG;
    if (strcmp(value, "EXTENDED") == 0) return SDL_TOUCH_TOP_PANEL_MODE_LONG;
    if (strcmp(value, "SHORT") == 0) return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
    return SDL_TOUCH_TOP_PANEL_MODE_SHORT;
}

static int normalize_touch_top_panel_tile_scale(int scale)
{
    if (scale <= 0)
        return SDL_TOUCH_TOP_PANEL_TILE_SCALE_DEFAULT;
    if (scale < SDL_TOUCH_TOP_PANEL_TILE_SCALE_MIN)
        return SDL_TOUCH_TOP_PANEL_TILE_SCALE_MIN;
    if (scale > SDL_TOUCH_TOP_PANEL_TILE_SCALE_MAX)
        return SDL_TOUCH_TOP_PANEL_TILE_SCALE_MAX;
    return scale;
}

static const char* touch_movement_mode_to_string(int mode)
{
    switch (mode) {
        case SDL_TOUCH_MOVEMENT_OFF: return "OFF";
        case SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY: return "LONG_PRESS_ONLY";
        case SDL_TOUCH_MOVEMENT_ON:
        default:
            return "ON";
    }
}

static int normalize_touch_movement_mode(int mode)
{
    if (mode == SDL_TOUCH_MOVEMENT_OFF
        || mode == SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY)
    {
        return mode;
    }

    return SDL_TOUCH_MOVEMENT_ON;
}

static int parse_touch_movement_mode(const char* value)
{
    if (!value)
        return SDL_TOUCH_MOVEMENT_ON;
    if (strcmp(value, "ON") == 0) return SDL_TOUCH_MOVEMENT_ON;
    if (strcmp(value, "OFF") == 0) return SDL_TOUCH_MOVEMENT_OFF;
    if (strcmp(value, "LONG_PRESS_ONLY") == 0) return SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY;
    if (strcmp(value, "LONG_CLICK_ONLY") == 0) return SDL_TOUCH_MOVEMENT_LONG_PRESS_ONLY;
    return SDL_TOUCH_MOVEMENT_ON;
}

static const char* touch_zone_overlay_mode_to_string(int mode)
{
    switch (mode) {
        case SDL_TOUCH_ZONE_OVERLAY_OFF: return "OFF";
        case SDL_TOUCH_ZONE_OVERLAY_BORDERS: return "BORDERS";
        case SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS: return "BORDERS_LABELS";
        case SDL_TOUCH_ZONE_OVERLAY_MARKERS:
        default:
            return "MARKERS";
    }
}

static int normalize_touch_zone_overlay_mode(int mode)
{
    if (mode >= SDL_TOUCH_ZONE_OVERLAY_OFF
        && mode < SDL_TOUCH_ZONE_OVERLAY_COUNT)
    {
        return mode;
    }

    return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
}

static int parse_touch_zone_overlay_mode(const char* value)
{
    if (!value)
        return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
    if (strcmp(value, "OFF") == 0 || strcmp(value, "NONE") == 0)
        return SDL_TOUCH_ZONE_OVERLAY_OFF;
    if (strcmp(value, "MARKERS") == 0 || strcmp(value, "SMALL_LINES") == 0)
        return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
    if (strcmp(value, "BORDERS") == 0 || strcmp(value, "FULL_BORDERS") == 0)
        return SDL_TOUCH_ZONE_OVERLAY_BORDERS;
    if (strcmp(value, "BORDERS_LABELS") == 0
        || strcmp(value, "FULL_BORDERS_LABELS") == 0
        || strcmp(value, "BORDERS_WITH_NAMES") == 0)
    {
        return SDL_TOUCH_ZONE_OVERLAY_BORDERS_LABELS;
    }
    return SDL_TOUCH_ZONE_OVERLAY_MARKERS;
}

static const char* touch_corner_up_down_side_to_string(int side)
{
    switch (side) {
        case SDL_TOUCH_CORNER_UP_DOWN_LEFT: return "LEFT";
        case SDL_TOUCH_CORNER_UP_DOWN_RIGHT:
        default:
            return "RIGHT";
    }
}

static int normalize_touch_corner_up_down_side(int side)
{
    if (side == SDL_TOUCH_CORNER_UP_DOWN_LEFT
        || side == SDL_TOUCH_CORNER_UP_DOWN_RIGHT)
    {
        return side;
    }

    return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
}

static int parse_touch_corner_up_down_side(const char* value)
{
    if (!value)
        return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
    if (strcmp(value, "LEFT") == 0) return SDL_TOUCH_CORNER_UP_DOWN_LEFT;
    if (strcmp(value, "RIGHT") == 0) return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
    return SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
}

static const char* mouse_movement_mode_to_string(int mode)
{
    switch (mode) {
        case SDL_MOUSE_MOVEMENT_OFF: return "OFF";
        case SDL_MOUSE_MOVEMENT_RIGHT_ONLY: return "RIGHT_ONLY";
        case SDL_MOUSE_MOVEMENT_ON:
        default:
            return "ON";
    }
}

static int normalize_mouse_movement_mode(int mode)
{
    if (mode == SDL_MOUSE_MOVEMENT_OFF
        || mode == SDL_MOUSE_MOVEMENT_RIGHT_ONLY)
    {
        return mode;
    }

    return SDL_MOUSE_MOVEMENT_ON;
}

static int parse_mouse_movement_mode(const char* value)
{
    if (!value)
        return SDL_MOUSE_MOVEMENT_ON;
    if (strcmp(value, "ON") == 0) return SDL_MOUSE_MOVEMENT_ON;
    if (strcmp(value, "OFF") == 0) return SDL_MOUSE_MOVEMENT_OFF;
    if (strcmp(value, "RIGHT_ONLY") == 0) return SDL_MOUSE_MOVEMENT_RIGHT_ONLY;
    if (strcmp(value, "RIGHT_CLICK_ONLY") == 0) return SDL_MOUSE_MOVEMENT_RIGHT_ONLY;
    return SDL_MOUSE_MOVEMENT_ON;
}

static int sdl_config_gamepad_action_binding_count(const struct sdl_config* config,
    int binding)
{
    int count = 0;

    if (!config)
        return 0;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (config->gamepad_button_bindings[i] == binding)
            count++;
    }

    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        if (config->gamepad_trigger_bindings[i] == binding)
            count++;
    }

    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        if (config->gamepad_left_stick_bindings[i] == binding)
            count++;
        if (config->gamepad_right_stick_bindings[i] == binding)
            count++;
    }

    for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
            if (config->gamepad_button_combo_bindings[modifier][i] == binding)
                count++;
        }
        for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
            if (config->gamepad_trigger_combo_bindings[modifier][i] == binding)
                count++;
        }
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            if (config->gamepad_left_stick_combo_bindings[modifier][i] == binding)
                count++;
            if (config->gamepad_right_stick_combo_bindings[modifier][i] == binding)
                count++;
        }
    }

    if (config->gamepad_shoulder_combo_binding == binding)
        count++;

    return count;
}

static bool sdl_config_gamepad_combo_bindings_empty(const struct sdl_config* config)
{
    if (!config)
        return true;

    for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
            if (config->gamepad_button_combo_bindings[modifier][i]
                != GAMEPAD_BIND_NONE)
                return false;
        }
        for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
            if (config->gamepad_trigger_combo_bindings[modifier][i]
                != GAMEPAD_BIND_NONE)
                return false;
        }
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            if (config->gamepad_left_stick_combo_bindings[modifier][i]
                != GAMEPAD_BIND_NONE)
                return false;
            if (config->gamepad_right_stick_combo_bindings[modifier][i]
                != GAMEPAD_BIND_NONE)
                return false;
        }
    }

    return true;
}

static bool sdl_config_should_upgrade_legacy_gamepad_defaults(
    const struct sdl_config* config)
{
    struct sdl_config defaults;

    if (!config)
        return false;

    memset(&defaults, 0, sizeof(defaults));
    sdl_config_set_default_gamepad_bindings(&defaults);

    if (memcmp(config->gamepad_button_bindings, defaults.gamepad_button_bindings,
            sizeof(defaults.gamepad_button_bindings)) != 0)
        return false;

    if (memcmp(config->gamepad_trigger_bindings, defaults.gamepad_trigger_bindings,
            sizeof(defaults.gamepad_trigger_bindings)) != 0)
        return false;

    if (memcmp(config->gamepad_left_stick_bindings,
            defaults.gamepad_left_stick_bindings,
            sizeof(defaults.gamepad_left_stick_bindings)) != 0)
        return false;

    if (memcmp(config->gamepad_right_stick_bindings,
            defaults.gamepad_right_stick_bindings,
            sizeof(defaults.gamepad_right_stick_bindings)) != 0)
        return false;

    if (config->gamepad_shoulder_combo_binding
        != defaults.gamepad_shoulder_combo_binding)
        return false;

    if (!sdl_config_gamepad_combo_bindings_empty(config))
        return false;

    if (config->steamdeck_inv_equip_same_button_cycle)
        return false;

    return true;
}

static char* read_file_contents(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        log_debug("Could not open JSON file: %s", filename);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        log_error("Failed to seek JSON file: %s", filename);
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0 || size > 16L * 1024L * 1024L) {
        log_error("Invalid JSON file size for %s: %ld", filename, size);
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        log_error("Failed to rewind JSON file: %s", filename);
        fclose(f);
        return NULL;
    }

    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        log_error("Failed to allocate memory for JSON file");
        return NULL;
    }

    size_t read_size = fread(content, 1, (size_t)size, f);
    if (read_size != (size_t)size && ferror(f))
        log_warn("Failed to read full JSON file: %s", filename);
    content[read_size] = '\0';
    fclose(f);

    return content;
}

static bool g_app_intro_seen = false;
static bool g_app_touch_tutorial_seen = false;
static bool g_app_mouse_tutorial_seen = false;
static bool g_app_character_wheel_tutorial_seen = false;
static bool g_app_keyboard_preset_prompt_seen = false;

static const byte app_input_options[] = {
    OPT_hjkl_movement, OPT_angband_keyset,
    OPT_NONE
};

static const byte app_interface_options[] = {
    OPT_look_objects_sort_by_difficulty, OPT_look_nearby_filter_default,
    OPT_song_list_sort_by_recent,
    OPT_show_level_generation_debug, OPT_show_elemental_item_rolls,
    OPT_supply_menu_random_icons,
    OPT_supply_menu_hide_flavor_compact, OPT_hide_secondary_action_ring,
    OPT_hide_supporting_panes_fullscreen,
    OPT_NONE
};

static const byte app_text_options[] = {
    OPT_story_object_desc, OPT_story_monster_desc,
    OPT_story_monster_desc_pane, OPT_story_lists_inven_pane,
    OPT_story_lists_equip_pane,
    OPT_NONE
};

static const byte app_gameplay_options[] = {
    OPT_active_weapon_switch_confirm,
    OPT_load_blitz_by_default,
    OPT_NONE
};

static const byte app_visual_options[] = {
    OPT_artifact_unique_color, OPT_hilite_player, OPT_hilite_target,
    OPT_hilite_unwary, OPT_solid_walls, OPT_hybrid_walls,
    OPT_unidentified_items_slate, OPT_stealth_vision, OPT_sleep_icon,
    OPT_mirror_player_tile_facing, OPT_handcrafted_player_tile_facing,
    OPT_mirror_monster_tile_facing,
    OPT_center_player, OPT_run_avoid_center,
    OPT_show_smithing_difficulty,
    OPT_show_smithing_difficulty_look, OPT_NONE
};

static bool option_list_contains(const byte* ids, int opt)
{
    if (!ids)
        return false;

    for (int i = 0; ids[i] != OPT_NONE; i++) {
        if ((int)ids[i] == opt)
            return true;
    }

    return false;
}

static bool option_is_retired_app_text_option(int opt)
{
    switch (opt) {
    case OPT_story_lists:
    case OPT_story_lists_inven:
    case OPT_story_lists_equip:
    case OPT_story_character_sheet:
        return true;
    default:
        return false;
    }
}

bool option_is_app_persistent(int opt)
{
    /* Multi-value non-bool options saved explicitly in the visual JSON block */
    if (opt == OPT_delay_factor || opt == OPT_running_delay
        || opt == OPT_hitpoint_warning
        || opt == OPT_intro_style || opt == OPT_show_level_entry_banner
        || opt == OPT_show_partition_narrative
        || opt == OPT_narrative_banner_turns)
        return true;
    if (option_is_retired_app_text_option(opt))
        return true;
    return option_list_contains(app_input_options, opt)
        || option_list_contains(app_interface_options, opt)
        || option_list_contains(app_text_options, opt)
        || option_list_contains(app_gameplay_options, opt)
        || option_list_contains(app_visual_options, opt);
}

static bool sdl_config_default_app_bool(int opt)
{
    if (opt == OPT_hide_supporting_panes_fullscreen)
        return true;

    /* Hide the action wheel's secondary ring by default everywhere except
     * touch-only devices, where the full wheel is shown at once. */
    if (opt == OPT_hide_secondary_action_ring)
        return !sdl_touch_only_device_active();

    if (opt >= 0 && opt < OPT_MAX)
        return option_norm[opt];

    return false;
}

static void sdl_config_apply_app_bool_defaults(const byte* option_ids)
{
    if (!op_ptr)
        return;

    /* Keep app-wide option defaults aligned with the canonical option table. */
    for (int i = 0; option_ids[i] != OPT_NONE; i++) {
        int opt = option_ids[i];

        if (opt >= 0 && opt < OPT_MAX)
            op_ptr->opt[opt] = sdl_config_default_app_bool(opt);
    }
}

static void sdl_config_apply_app_option_defaults(void)
{
    if (!op_ptr)
        return;

    sdl_config_apply_app_bool_defaults(app_input_options);
    sdl_config_apply_app_bool_defaults(app_interface_options);
    sdl_config_apply_app_bool_defaults(app_text_options);
    sdl_config_apply_app_bool_defaults(app_gameplay_options);
    sdl_config_apply_app_bool_defaults(app_visual_options);

    op_ptr->delay_factor = 5;
    op_ptr->running_delay_ms = DEFAULT_RUNNING_DELAY_MS;
    op_ptr->hitpoint_warn = 3;
    op_ptr->main_combat_rolls = 0;
    op_ptr->intro_style = INTRO_STYLE_RANDOM;
    op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
    op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER_DELAY;
    op_ptr->narrative_banner_turns = DEFAULT_NARRATIVE_BANNER_TURNS;
}

void sdl_config_reset_app_options_to_defaults(void)
{
    sdl_config_apply_app_option_defaults();
}

static void sdl_config_load_app_option_group(cJSON* app_options,
    const char* group_name, const byte* option_ids)
{
    cJSON* group = cJSON_GetObjectItemCaseSensitive(app_options, group_name);
    if (!op_ptr)
        return;

    /* Old sil_sdl.json files may not contain newly added options. Reset each
     * key to its default before loading any stored override. */
    for (int i = 0; option_ids[i] != OPT_NONE; i++) {
        int opt = option_ids[i];
        cptr key = option_text[opt];
        cJSON* item;

        if (opt >= 0 && opt < OPT_MAX)
            op_ptr->opt[opt] = sdl_config_default_app_bool(opt);

        if (!cJSON_IsObject(group))
            continue;

        if (!key)
            continue;

        item = cJSON_GetObjectItemCaseSensitive(group, key);
        if (cJSON_IsBool(item))
            op_ptr->opt[opt] = cJSON_IsTrue(item);
    }
}

static bool sdl_config_try_load_app_bool_option(cJSON* app_options,
    const char* group_name, int opt)
{
    cJSON* group;
    cJSON* item;
    cptr key;

    if (!op_ptr || !cJSON_IsObject(app_options) || !group_name
        || opt < 0 || opt >= OPT_MAX)
        return false;

    key = option_text[opt];
    if (!key)
        return false;

    group = cJSON_GetObjectItemCaseSensitive(app_options, group_name);
    if (!cJSON_IsObject(group))
        return false;

    item = cJSON_GetObjectItemCaseSensitive(group, key);
    if (!cJSON_IsBool(item))
        return false;

    op_ptr->opt[opt] = cJSON_IsTrue(item);
    return true;
}

static void sdl_config_save_app_option_group(cJSON* app_options,
    const char* group_name, const byte* option_ids)
{
    if (!op_ptr)
        return;

    cJSON* group = cJSON_CreateObject();
    if (!group)
        return;

    for (int i = 0; option_ids[i] != OPT_NONE; i++) {
        int opt = option_ids[i];
        cptr key = option_text[opt];

        if (!key)
            continue;

        cJSON_AddBoolToObject(group, key, op_ptr->opt[opt]);
    }

    cJSON_AddItemToObject(app_options, group_name, group);
}

static bool sdl_config_try_load_byte_value(cJSON* parent, const char* key,
    byte* out_value, byte max_value);

static void sdl_config_load_byte_value(cJSON* parent, const char* key,
    byte* out_value, byte max_value, byte default_value)
{
    if (!out_value)
        return;

    /* Missing numeric keys are deliberate defaults, not inherited state. */
    *out_value = MIN(default_value, max_value);

    (void)sdl_config_try_load_byte_value(parent, key, out_value, max_value);
}

static bool sdl_config_try_load_byte_value(cJSON* parent, const char* key,
    byte* out_value, byte max_value)
{
    cJSON* item;

    if (!out_value)
        return false;

    if (!cJSON_IsObject(parent))
        return false;

    item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!cJSON_IsNumber(item))
        return false;

    if (item->valueint < 0)
        return false;

    if (item->valueint > max_value) {
        *out_value = max_value;
        return true;
    }

    *out_value = (byte)item->valueint;
    return true;
}

void sdl_config_load_app_options(const char* filename)
{
    char* content;
    cJSON* root;
    cJSON* app_options;
    cJSON* item;
    bool config_exists = false;

    sdl_config_apply_app_option_defaults();

    if (filename && filename[0])
        config_exists = SDL_GetPathInfo(filename, NULL);

    g_app_intro_seen = config_exists;
    g_app_touch_tutorial_seen = false;
    g_app_mouse_tutorial_seen = false;
    g_app_character_wheel_tutorial_seen = false;
    g_app_keyboard_preset_prompt_seen = false;

    if (!filename || !filename[0]) {
        log_warn("sdl_config_load_app_options: no config filename provided");
        return;
    }

    content = read_file_contents(filename);
    if (!content) {
        log_debug("No app options found in SDL config, using defaults");
        return;
    }

    root = cJSON_Parse(content);
    free(content);

    if (!root) {
        log_warn("sdl_config_load_app_options: failed to parse %s", filename);
        return;
    }

    app_options = cJSON_GetObjectItemCaseSensitive(root, "appOptions");
    if (!cJSON_IsObject(app_options)) {
        cJSON_Delete(root);
        return;
    }

    if (!op_ptr) {
        cJSON_Delete(root);
        return;
    }

    item = cJSON_GetObjectItemCaseSensitive(app_options, "introSeen");
    if (cJSON_IsBool(item))
        g_app_intro_seen = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "touchTutorialSeen");
    if (cJSON_IsBool(item))
        g_app_touch_tutorial_seen = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "mouseTutorialSeen");
    if (cJSON_IsBool(item))
        g_app_mouse_tutorial_seen = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(app_options,
        "characterWheelTutorialSeen");
    if (cJSON_IsBool(item))
        g_app_character_wheel_tutorial_seen = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(app_options,
        "keyboardPresetPromptSeen");
    if (cJSON_IsBool(item))
        g_app_keyboard_preset_prompt_seen = cJSON_IsTrue(item);

    sdl_config_load_app_option_group(app_options, "input", app_input_options);
    sdl_config_load_app_option_group(app_options, "interface", app_interface_options);
    sdl_config_load_app_option_group(app_options, "text", app_text_options);
    sdl_config_load_app_option_group(app_options, "gameplay", app_gameplay_options);
    sdl_config_load_app_option_group(app_options, "visual", app_visual_options);

    /* Migrate settings from their pre-regrouping JSON owners. */
    if (!sdl_config_try_load_app_bool_option(app_options, "input",
            OPT_hjkl_movement))
        sdl_config_try_load_app_bool_option(app_options, "interface",
            OPT_hjkl_movement);
    if (!sdl_config_try_load_app_bool_option(app_options, "input",
            OPT_angband_keyset))
        sdl_config_try_load_app_bool_option(app_options, "interface",
            OPT_angband_keyset);
    if (!sdl_config_try_load_app_bool_option(app_options, "interface",
            OPT_hide_supporting_panes_fullscreen))
        sdl_config_try_load_app_bool_option(app_options, "efficiency",
            OPT_hide_supporting_panes_fullscreen);
    if (!sdl_config_try_load_app_bool_option(app_options, "visual",
            OPT_center_player))
        sdl_config_try_load_app_bool_option(app_options, "efficiency",
            OPT_center_player);
    if (!sdl_config_try_load_app_bool_option(app_options, "visual",
            OPT_run_avoid_center))
        sdl_config_try_load_app_bool_option(app_options, "efficiency",
            OPT_run_avoid_center);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "interface");
    sdl_config_load_byte_value(item, "hitpointWarning", &op_ptr->hitpoint_warn,
        9, 3);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "visual");
    op_ptr->delay_factor = 5;
    if (!sdl_config_try_load_byte_value(item, "delayFactor",
            &op_ptr->delay_factor, 9))
    {
        cJSON* legacy_efficiency = cJSON_GetObjectItemCaseSensitive(
            app_options, "efficiency");
        sdl_config_try_load_byte_value(legacy_efficiency, "delayFactor",
            &op_ptr->delay_factor, 9);
    }

    op_ptr->running_delay_ms = DEFAULT_RUNNING_DELAY_MS;
    if (!sdl_config_try_load_byte_value(item, "runningDelayMs",
            &op_ptr->running_delay_ms, MAX_RUNNING_DELAY_MS))
    {
        cJSON* legacy_efficiency = cJSON_GetObjectItemCaseSensitive(
            app_options, "efficiency");
        cJSON* legacy_instant = cJSON_IsObject(legacy_efficiency)
            ? cJSON_GetObjectItemCaseSensitive(legacy_efficiency, "instant_run")
            : NULL;

        if (!sdl_config_try_load_byte_value(legacy_efficiency,
                "runningDelayMs", &op_ptr->running_delay_ms,
                MAX_RUNNING_DELAY_MS)
            && cJSON_IsBool(legacy_instant))
        {
            op_ptr->running_delay_ms = cJSON_IsTrue(legacy_instant)
                ? 0 : DEFAULT_RUNNING_DELAY_MS;
        }
    }

    sdl_config_load_byte_value(item, "introStyle", &op_ptr->intro_style,
        INTRO_STYLE_RANDOM, INTRO_STYLE_RANDOM);
    sdl_config_load_byte_value(item, "levelEntryNarrativeMode",
        &op_ptr->level_entry_narrative_mode, LEVEL_ENTRY_NARRATIVE_OFF,
        LEVEL_ENTRY_NARRATIVE_BANNER_DELAY);
    sdl_config_load_byte_value(item, "partitionNarrativeMode",
        &op_ptr->partition_narrative_mode, PARTITION_NARRATIVE_BANNER_DELAY,
        PARTITION_NARRATIVE_BANNER_DELAY);
    sdl_config_load_byte_value(item, "narrativeBannerTurns",
        &op_ptr->narrative_banner_turns, NARRATIVE_BANNER_TURNS_MAX,
        DEFAULT_NARRATIVE_BANNER_TURNS);

    cJSON_Delete(root);
}

bool sdl_config_should_force_intro_flame(void)
{
    return !g_app_intro_seen;
}

void sdl_config_mark_intro_seen(void)
{
    g_app_intro_seen = true;
}

bool sdl_config_touch_tutorial_seen(void)
{
    return g_app_touch_tutorial_seen;
}

void sdl_config_mark_touch_tutorial_seen(void)
{
    g_app_touch_tutorial_seen = true;
}

bool sdl_config_mouse_tutorial_seen(void)
{
    return g_app_mouse_tutorial_seen;
}

void sdl_config_mark_mouse_tutorial_seen(void)
{
    g_app_mouse_tutorial_seen = true;
}

bool sdl_config_character_wheel_tutorial_seen(void)
{
    return g_app_character_wheel_tutorial_seen;
}

void sdl_config_mark_character_wheel_tutorial_seen(void)
{
    g_app_character_wheel_tutorial_seen = true;
}

bool sdl_config_keyboard_preset_prompt_seen(void)
{
    return g_app_keyboard_preset_prompt_seen;
}

void sdl_config_mark_keyboard_preset_prompt_seen(void)
{
    g_app_keyboard_preset_prompt_seen = true;
}

static void sdl_config_load_touch_binding_array(cJSON* array, int* dst, int max_count)
{
    int count;

    if (!cJSON_IsArray(array) || !dst || max_count <= 0)
        return;

    count = cJSON_GetArraySize(array);
    for (int i = 0; i < max_count && i < count; i++) {
        cJSON* binding = cJSON_GetArrayItem(array, i);
        if (cJSON_IsNumber(binding))
            dst[i] = binding->valueint;
    }
}

static void sdl_config_load_touch_label_array(cJSON* array, char dst[][SDL_TOUCH_PANE_LABEL_LEN], int max_count)
{
    int count;

    if (!cJSON_IsArray(array) || !dst || max_count <= 0)
        return;

    count = cJSON_GetArraySize(array);
    for (int i = 0; i < max_count && i < count; i++) {
        cJSON* label = cJSON_GetArrayItem(array, i);
        if (cJSON_IsString(label) && label->valuestring) {
            SDL_strlcpy(dst[i], label->valuestring, SDL_TOUCH_PANE_LABEL_LEN);
        }
    }
}

static void sdl_config_migrate_touch_pane_binding(struct sdl_config* config,
    int panel, int index, int old_binding, int new_binding, cptr message)
{
    int* bindings;
    char (*labels)[SDL_TOUCH_PANE_LABEL_LEN];

    if (!config)
        return;
    if (panel < 0 || panel >= SDL_TOUCH_PANE_PANEL_COUNT)
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND) {
        bindings = config->touch_pane_second_bindings;
        labels = config->touch_pane_second_labels;
    } else {
        bindings = config->touch_pane_bindings;
        labels = config->touch_pane_labels;
    }

    if (bindings[index] != old_binding || labels[index][0])
        return;

    bindings[index] = new_binding;
    if (message && message[0])
        log_info("%s", message);
}

static void sdl_config_set_default_top_panel_bindings(struct sdl_config* config)
{
    /* Left to right: Quaff potions, Inventory, Abilities, Hints, Character
     * sheet, ASCII/Tiles toggle, Smithing, Look. */
    static const int top_panel_defaults[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {
        'q', 'i', 'y', TOUCH_BIND_MAIN_MENU_HINTS_QUESTS,
        'h', TOUCH_BIND_TOGGLE_TILES, '0', 'l',
    };
    static const int top_panel_long_defaults[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {
        GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE,
        GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE,
        GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE,
    };

    if (!config)
        return;

    memcpy(config->touch_top_panel_bindings, top_panel_defaults,
        sizeof(top_panel_defaults));
    memcpy(config->touch_top_panel_long_bindings, top_panel_long_defaults,
        sizeof(top_panel_long_defaults));
}

static void sdl_config_set_default_thumb_bindings(struct sdl_config* config)
{
    /* Button 1: tap = space (pick/continue), long = 'x' (look/describe).
     * Button 2: tap = 'z' (wait), long = 'Z' (rest). */
    static const int thumb_defaults[SDL_TOUCH_THUMB_BUTTON_COUNT] = {
        ' ', 'z',
    };
    static const int thumb_long_defaults[SDL_TOUCH_THUMB_BUTTON_COUNT] = {
        'x', 'Z',
    };

    if (!config)
        return;

    memcpy(config->touch_thumb_bindings, thumb_defaults,
        sizeof(thumb_defaults));
    memcpy(config->touch_thumb_long_bindings, thumb_long_defaults,
        sizeof(thumb_long_defaults));
}

static void sdl_config_migrate_touch_top_panel_defaults(
    struct sdl_config* config)
{
    static const int previous_taps[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {
        'z', 'h', 'i', 'a', 'l', 'f', GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE,
    };
    static const int previous_longs[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT] = {
        'Z', '\t', 'e', 'p', 'j', 'F', GAMEPAD_BIND_NONE, GAMEPAD_BIND_NONE,
    };

    if (!config)
        return;

    if (memcmp(config->touch_top_panel_bindings, previous_taps,
            sizeof(previous_taps)) != 0)
    {
        return;
    }
    if (memcmp(config->touch_top_panel_long_bindings, previous_longs,
            sizeof(previous_longs)) != 0)
    {
        return;
    }

    sdl_config_set_default_top_panel_bindings(config);
    log_info("Migrated default quick access buttons to icon layout");
}

static void sdl_config_migrate_touch_top_panel_layout(
    struct sdl_config* config, int tap_count, int long_count)
{
    int old_taps[4] = { 0 };
    int old_longs[4] = { 0 };
    int saved_taps[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    int saved_longs[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
    bool old_default_taps;
    bool old_default_longs;

    if (!config)
        return;

    memcpy(saved_taps, config->touch_top_panel_bindings, sizeof(saved_taps));
    memcpy(saved_longs, config->touch_top_panel_long_bindings,
        sizeof(saved_longs));
    if (tap_count == 4)
        memcpy(old_taps, config->touch_top_panel_bindings, sizeof(old_taps));
    if (long_count == 4)
        memcpy(old_longs, config->touch_top_panel_long_bindings,
            sizeof(old_longs));

    old_default_taps = tap_count == 4
        && config->touch_top_panel_bindings[0] == 'h'
        && config->touch_top_panel_bindings[1] == 'i'
        && config->touch_top_panel_bindings[2] == 'j'
        && config->touch_top_panel_bindings[3] == 'f';
    old_default_longs = long_count == 4
        && config->touch_top_panel_long_bindings[0] == '\t'
        && config->touch_top_panel_long_bindings[1] == 'e'
        && config->touch_top_panel_long_bindings[2] == 's'
        && config->touch_top_panel_long_bindings[3] == 'F';

    if (old_default_taps && old_default_longs) {
        sdl_config_set_default_top_panel_bindings(config);
        log_info("Migrated default quick access buttons to icon layout");
        return;
    }

    if (tap_count == 4 || long_count == 4) {
        sdl_config_set_default_top_panel_bindings(config);
        if (tap_count != 4)
            memcpy(config->touch_top_panel_bindings, saved_taps,
                sizeof(saved_taps));
        if (long_count != 4)
            memcpy(config->touch_top_panel_long_bindings, saved_longs,
                sizeof(saved_longs));
    }

    if (tap_count == 4) {
        for (int i = 0; i < 4; i++)
            config->touch_top_panel_bindings[i + 1] = old_taps[i];
        if (long_count != 4)
            log_info("Shifted four-button quick access tap bindings into short layout");
    }

    if (long_count == 4) {
        for (int i = 0; i < 4; i++)
            config->touch_top_panel_long_bindings[i + 1] = old_longs[i];
        log_info((tap_count == 4)
            ? "Shifted four-button quick access bindings into short layout"
            : "Shifted four-button quick access long-tap bindings into short layout");
    }
}

static cJSON* sdl_config_create_int_array(const int* src, int count)
{
    cJSON* array;

    if (!src || count <= 0)
        return NULL;

    array = cJSON_CreateArray();
    if (!array)
        return NULL;

    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateNumber(src[i]));
    }

    return array;
}

static void sdl_config_clear_gamepad_combo_bindings(struct sdl_config* config)
{
    if (!config)
        return;

    for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
            config->gamepad_button_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++)
            config->gamepad_trigger_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            config->gamepad_left_stick_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
            config->gamepad_right_stick_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        }
    }
}

static const char* sdl_config_gamepad_button_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftButtonBindings",
    "ctrlButtonBindings",
    "altButtonBindings",
};

static const char* sdl_config_gamepad_trigger_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftTriggerBindings",
    "ctrlTriggerBindings",
    "altTriggerBindings",
};

static const char* sdl_config_gamepad_left_stick_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftLeftStickBindings",
    "ctrlLeftStickBindings",
    "altLeftStickBindings",
};

static const char* sdl_config_gamepad_right_stick_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftRightStickBindings",
    "ctrlRightStickBindings",
    "altRightStickBindings",
};

static void sdl_config_copy_pane_configs(struct pane_config* dest, int* dest_count,
    const struct pane_config* src, int src_count)
{
    int count = src_count;

    if (!dest || !dest_count)
        return;

    if (count < 0)
        count = 0;
    if (count > MAX_PANE_CONFIGS)
        count = MAX_PANE_CONFIGS;

    if (count > 0 && src)
        memcpy(dest, src, sizeof(struct pane_config) * count);

    if (count < MAX_PANE_CONFIGS)
        memset(dest + count, 0, sizeof(struct pane_config) * (MAX_PANE_CONFIGS - count));

    *dest_count = count;
}

static void sdl_config_copy_pane_profile(struct sdl_pane_profile* dest,
    const struct sdl_pane_profile* src)
{
    if (!dest || !src)
        return;

    dest->main_view_scale = src->main_view_scale;
    dest->aux_view_font_size = src->aux_view_font_size;
    dest->enable_right_panes = src->enable_right_panes;
    dest->enable_bottom_panes = src->enable_bottom_panes;
    sdl_config_copy_pane_configs(dest->pane_configs, &dest->pane_count,
        src->pane_configs, src->pane_count);
}

static void sdl_config_load_pane_array(cJSON* panes, struct pane_config* pane_configs,
    int* pane_count, int max_panes, const char* label)
{
    int count = 0;
    cJSON* pane_item = NULL;
    int array_size;

    if (!pane_count)
        return;

    *pane_count = 0;

    if (!cJSON_IsArray(panes)) {
        if (label)
            log_warn("'%s' array not found in JSON", label);
        return;
    }

    array_size = cJSON_GetArraySize(panes);
    if (label)
        log_debug("Found '%s' array with %d items", label, array_size);

    cJSON_ArrayForEach(pane_item, panes) {
        struct pane_config* pc;
        cJSON* type;
        cJSON* where;
        cJSON* enabled;
        cJSON* rows;
        cJSON* cols;
        cJSON* ratio;
        cJSON* font_size;

        if (count >= max_panes) {
            log_warn("Too many panes in config, maximum is %d", max_panes);
            break;
        }

        pc = &pane_configs[count];
        memset(pc, 0, sizeof(*pc));
        pc->pane = PANE_MAIN;
        pc->enabled = true;

        type = cJSON_GetObjectItemCaseSensitive(pane_item, "type");
        if (cJSON_IsString(type)) {
            pc->pane = parse_pane_type(type->valuestring);
            log_debug("Pane %d: type=%s", count, type->valuestring);
        }

        where = cJSON_GetObjectItemCaseSensitive(pane_item, "where");
        if (cJSON_IsString(where)) {
            pc->where = parse_pane_placement(where->valuestring);
            log_debug("Pane %d: where=%s", count, where->valuestring);
        }

        enabled = cJSON_GetObjectItemCaseSensitive(pane_item, "enabled");
        if (cJSON_IsBool(enabled)) {
            pc->enabled = cJSON_IsTrue(enabled);
            log_debug("Pane %d: enabled=%s", count, pc->enabled ? "true" : "false");
        }

        rows = cJSON_GetObjectItemCaseSensitive(pane_item, "rows");
        if (cJSON_IsNumber(rows)) {
            pc->rect.rows = rows->valueint;
            log_debug("Pane %d: rows=%d", count, pc->rect.rows);
        }

        cols = cJSON_GetObjectItemCaseSensitive(pane_item, "cols");
        if (cJSON_IsNumber(cols)) {
            pc->rect.cols = cols->valueint;
            log_debug("Pane %d: cols=%d", count, pc->rect.cols);
        }

        ratio = cJSON_GetObjectItemCaseSensitive(pane_item, "ratio");
        if (cJSON_IsNumber(ratio)) {
            pc->ratio = (float)ratio->valuedouble;
            log_debug("Pane %d: ratio=%.2f", count, pc->ratio);
        }

        font_size = cJSON_GetObjectItemCaseSensitive(pane_item, "fontSize");
        if (cJSON_IsNumber(font_size)) {
            pc->font_size = font_size->valueint;
            if (pc->font_size < 0)
                pc->font_size = 0;
            if (pc->font_size > 48)
                pc->font_size = 48;
            log_debug("Pane %d: fontSize=%d", count, pc->font_size);
        }

        if (!pane_type_allows_placement(pc->pane, pc->where)) {
            enum pane_placement fallback = pane_first_allowed_placement(pc->pane);
            log_warn("Pane %d placement %s is invalid for type %s, using %s",
                count,
                pane_placement_name(pc->where),
                pane_type_to_string(pc->pane),
                pane_placement_name(fallback));
            pc->where = fallback;
        }

        count++;
    }

    *pane_count = count;
    if (label)
        log_debug("Parsed %d panes from %s", count, label);
}

static void sdl_config_init_pane_profiles_from_legacy(const struct sdl_config* config,
    struct sdl_pane_profile* pane_profiles, int profile_count,
    const struct pane_config* pane_configs, int pane_count)
{
    if (!pane_profiles || profile_count <= 0)
        return;

    for (int mode = 0; mode < profile_count; mode++) {
        pane_profiles[mode].main_view_scale = config->main_view_scale;
        pane_profiles[mode].aux_view_font_size = config->aux_view_font_size;
        pane_profiles[mode].enable_right_panes = config->enable_right_panes;
        pane_profiles[mode].enable_bottom_panes = config->enable_bottom_panes;
        if (pane_count > 0) {
            sdl_config_copy_pane_configs(pane_profiles[mode].pane_configs,
                &pane_profiles[mode].pane_count, pane_configs, pane_count);
        }
    }
}

static void sdl_config_load_pane_profile(cJSON* profile_obj,
    struct sdl_pane_profile* profile, const char* label)
{
    cJSON* item;
    struct pane_config panes[MAX_PANE_CONFIGS] = { 0 };
    int pane_count = 0;

    if (!cJSON_IsObject(profile_obj) || !profile)
        return;

    item = cJSON_GetObjectItemCaseSensitive(profile_obj, "mainViewScale");
    if (cJSON_IsNumber(item)) {
        profile->main_view_scale = item->valueint;
        if (profile->main_view_scale < SDL_MAIN_VIEW_MIN_SCALE)
            profile->main_view_scale = SDL_MAIN_VIEW_MIN_SCALE;
    }

    item = cJSON_GetObjectItemCaseSensitive(profile_obj, "auxViewFontSize");
    if (cJSON_IsNumber(item))
        profile->aux_view_font_size = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(profile_obj, "enableRightPanes");
    if (cJSON_IsBool(item))
        profile->enable_right_panes = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(profile_obj, "enableBottomPanes");
    if (cJSON_IsBool(item))
        profile->enable_bottom_panes = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(profile_obj, "panes");
    if (cJSON_IsArray(item)) {
        sdl_config_load_pane_array(item, panes, &pane_count, MAX_PANE_CONFIGS, label);
        sdl_config_copy_pane_configs(profile->pane_configs, &profile->pane_count,
            panes, pane_count);
    }
}

static int sdl_config_profile_find_pane(struct sdl_pane_profile* profile,
    enum pane_type pane)
{
    if (!profile)
        return -1;

    for (int i = 0; i < profile->pane_count && i < MAX_PANE_CONFIGS; i++) {
        if (profile->pane_configs[i].pane == pane)
            return i;
    }

    return -1;
}

static bool sdl_config_profile_has_enabled_bottom_pane(
    const struct sdl_pane_profile* profile)
{
    if (!profile)
        return false;

    for (int i = 0; i < profile->pane_count && i < MAX_PANE_CONFIGS; i++) {
        const struct pane_config* pane = &profile->pane_configs[i];

        if (pane->pane == PANE_TOUCH)
            continue;
        if (pane->pane == PANE_ROLLS)
            continue;
        if (pane->enabled && pane_placement_is_bottom(pane->where))
            return true;
    }

    return false;
}

static bool sdl_config_profile_enable_default_log_pane(
    struct sdl_pane_profile* profile)
{
    struct pane_config* log_pane;
    int index;
    bool changed = false;

    if (!profile)
        return false;

    if (profile->pane_count < 0)
        profile->pane_count = 0;
    if (profile->pane_count > MAX_PANE_CONFIGS)
        profile->pane_count = MAX_PANE_CONFIGS;

    index = sdl_config_profile_find_pane(profile, PANE_LOG);
    if (index < 0) {
        if (profile->pane_count >= MAX_PANE_CONFIGS)
            return false;

        index = profile->pane_count++;
        memset(&profile->pane_configs[index], 0,
            sizeof(profile->pane_configs[index]));
        profile->pane_configs[index].pane = PANE_LOG;
        changed = true;
    }

    log_pane = &profile->pane_configs[index];
    if (log_pane->where == 0 || !pane_type_allows_placement(PANE_LOG,
            log_pane->where))
    {
        log_pane->where = PLACE_BOTTOM;
        changed = true;
    }
    if (!log_pane->enabled) {
        log_pane->enabled = true;
        changed = true;
    }
    if (log_pane->rect.rows <= 0) {
        log_pane->rect.rows = SDL_CONFIG_DEFAULT_LOG_PANE_ROWS;
        changed = true;
    }
    if (log_pane->rect.cols != 0) {
        log_pane->rect.cols = 0;
        changed = true;
    }
    if (!profile->enable_bottom_panes) {
        profile->enable_bottom_panes = true;
        changed = true;
    }

    return changed;
}

static void sdl_config_migrate_default_log_pane(struct sdl_config* config,
    struct sdl_pane_profile* pane_profiles, int profile_count)
{
    bool changed = false;

    if (!config || !config->enable_bottom_panes)
        return;

    if (!pane_profiles || profile_count <= 0)
        return;

    for (int mode = 0; mode < profile_count; mode++) {
        if (sdl_config_profile_has_enabled_bottom_pane(&pane_profiles[mode])) {
            if (!pane_profiles[mode].enable_bottom_panes) {
                pane_profiles[mode].enable_bottom_panes = true;
                changed = true;
            }
            continue;
        }

        if (sdl_config_profile_enable_default_log_pane(&pane_profiles[mode]))
            changed = true;
    }

    if (changed) {
        log_info("Migrated SDL config to default %d-row log pane",
            SDL_CONFIG_DEFAULT_LOG_PANE_ROWS);
    }
}

static cJSON* sdl_config_create_panes_array(const struct pane_config* pane_configs, int pane_count)
{
    cJSON* panes = cJSON_CreateArray();

    if (!panes)
        return NULL;

    for (int i = 0; i < pane_count; i++) {
        const struct pane_config* pc = &pane_configs[i];
        cJSON* pane = cJSON_CreateObject();

        if (!pane)
            continue;

        cJSON_AddStringToObject(pane, "type", pane_type_to_string(pc->pane));
        cJSON_AddStringToObject(pane, "where", pane_placement_to_string(pc->where));
        cJSON_AddBoolToObject(pane, "enabled", pc->enabled);

        if (pc->rect.rows > 0)
            cJSON_AddNumberToObject(pane, "rows", pc->rect.rows);

        if (pc->rect.cols > 0)
            cJSON_AddNumberToObject(pane, "cols", pc->rect.cols);

        if (pc->ratio > 0.0f)
            cJSON_AddNumberToObject(pane, "ratio", pc->ratio);

        if (pc->font_size > 0)
            cJSON_AddNumberToObject(pane, "fontSize", pc->font_size);

        cJSON_AddItemToArray(panes, pane);
    }

    return panes;
}

enum sdl_config_load_status sdl_config_load(const char* filename,
    struct sdl_config* config, struct sdl_pane_profile* pane_profiles,
    int profile_count)
{
    struct pane_config legacy_panes[MAX_PANE_CONFIGS] = { 0 };
    int legacy_pane_count = 0;
    bool saw_enable_bottom_panes = false;
    bool saw_left_panel_expanded_on_launch = false;

    /* Loaders overlay JSON onto defaults so old configs inherit new settings. */
    sdl_config_set_defaults(config);

    log_info("Loading SDL configuration from: %s", filename);
    
    char* content = read_file_contents(filename);
    if (!content) {
        log_debug("Failed to read config file, using defaults");
        return SDL_CONFIG_LOAD_READ_FAILED;
    }
    
    log_debug("Config file content length: %zu bytes", strlen(content));
    
    cJSON* root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            log_error("JSON parse error before: %s", error_ptr);
        } else {
            log_error("JSON parse error (no error pointer available)");
        }
        return SDL_CONFIG_LOAD_PARSE_FAILED;
    }
    
    log_debug("JSON parsed successfully");

    sdl_config_load_keyboard_keymaps(root, config);
    sdl_config_load_movement_bindings(root, config);
    
    // Parse SDL settings
    cJSON* sdl = cJSON_GetObjectItemCaseSensitive(root, "sdl");
    if (cJSON_IsObject(sdl)) {
        log_debug("Found 'sdl' object in JSON");
        cJSON* item;
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "mainViewScale");
        if (cJSON_IsNumber(item)) {
            config->main_view_scale = item->valueint;
            if (config->main_view_scale < SDL_MAIN_VIEW_MIN_SCALE)
                config->main_view_scale = SDL_MAIN_VIEW_MIN_SCALE;
            log_debug("Loaded mainViewScale: %d", config->main_view_scale);
        } else {
            log_warn("mainViewScale not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "auxViewFontSize");
        if (cJSON_IsNumber(item)) {
            config->aux_view_font_size = item->valueint;
            log_debug("Loaded auxViewFontSize: %d", config->aux_view_font_size);
        } else {
            log_warn("auxViewFontSize not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "margin");
        if (cJSON_IsNumber(item)) {
            config->margin = item->valueint;
            log_debug("Loaded margin: %d", config->margin);
        } else {
            log_warn("margin not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "fullscreen");
        if (cJSON_IsBool(item)) {
            config->fullscreen = cJSON_IsTrue(item);
            log_debug("Loaded fullscreen: %s", config->fullscreen ? "true" : "false");
        } else {
            log_warn("fullscreen not found or not a boolean");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "tiles");
        if (cJSON_IsBool(item)) {
            config->tiles = cJSON_IsTrue(item);
            log_debug("Loaded tiles: %s", config->tiles ? "true" : "false");
        } else {
            log_warn("tiles not found or not a boolean");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "palettePreset");
        if (cJSON_IsString(item) && item->valuestring) {
            SDL_strlcpy(config->palette_preset, item->valuestring,
                sizeof(config->palette_preset));
            log_debug("Loaded palettePreset: %s", config->palette_preset);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "useUnsafeArea");
        if (cJSON_IsBool(item)) {
            config->use_unsafe_area = cJSON_IsTrue(item);
            log_debug("Loaded useUnsafeArea: %s",
                config->use_unsafe_area ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "enableRightPanes");
        if (cJSON_IsBool(item)) {
            config->enable_right_panes = cJSON_IsTrue(item);
            log_debug("Loaded enableRightPanes: %s", config->enable_right_panes ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "enableBottomPanes");
        saw_enable_bottom_panes = (item != NULL);
        if (cJSON_IsBool(item)) {
            config->enable_bottom_panes = cJSON_IsTrue(item);
            log_debug("Loaded enableBottomPanes: %s", config->enable_bottom_panes ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "showPaneBorders");
        if (cJSON_IsBool(item)) {
            config->show_pane_borders = cJSON_IsTrue(item);
            log_debug("Loaded showPaneBorders: %s", config->show_pane_borders ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "leftPanelExpandedOnLaunch");
        saw_left_panel_expanded_on_launch = (item != NULL);
        if (cJSON_IsBool(item)) {
            config->left_panel_expanded_on_launch = cJSON_IsTrue(item);
            log_debug("Loaded leftPanelExpandedOnLaunch: %s",
                config->left_panel_expanded_on_launch ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "leftPanelCompactMode");
        if (cJSON_IsString(item)) {
            config->left_panel_compact_mode =
                parse_left_panel_compact_mode(item->valuestring);
            log_debug("Loaded leftPanelCompactMode: %s",
                left_panel_compact_mode_to_string(
                    config->left_panel_compact_mode));
        } else if (cJSON_IsNumber(item)) {
            config->left_panel_compact_mode =
                (item->valueint == SDL_LEFT_PANEL_COMPACT_ROW)
                    ? SDL_LEFT_PANEL_COMPACT_ROW
                    : SDL_LEFT_PANEL_COMPACT_COLUMN;
            log_debug("Loaded numeric leftPanelCompactMode: %s",
                left_panel_compact_mode_to_string(
                    config->left_panel_compact_mode));
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "minTerminalMode");
        if (cJSON_IsString(item)) {
            config->min_terminal_mode = parse_min_terminal_mode(item->valuestring);
            log_debug("Loaded minTerminalMode: %s", min_terminal_mode_to_string(config->min_terminal_mode));
        } else if (cJSON_IsNumber(item)) {
            if (item->valueint == SDL_MIN_TERMINAL_COMPACT)
                config->min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
            else
                config->min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
            log_debug("Loaded numeric minTerminalMode: %s", min_terminal_mode_to_string(config->min_terminal_mode));
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "logPaneDisplayFilter");
        if (cJSON_IsNumber(item)) {
            if (item->valueint >= LOG_HISTORY_FILTER_ALL
                && item->valueint <= LOG_HISTORY_FILTER_COMBAT)
            {
                config->log_pane_display_filter = item->valueint;
            }
            else
            {
                config->log_pane_display_filter = LOG_HISTORY_FILTER_ALL;
            }
            log_debug("Loaded logPaneDisplayFilter: %d",
                config->log_pane_display_filter);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "diceRollLockMs");
        if (cJSON_IsNumber(item)) {
            config->dice_roll_lock_ms = item->valueint;
            if (config->dice_roll_lock_ms < 0)
                config->dice_roll_lock_ms = 0;
            if (config->dice_roll_lock_ms > SDL_DICE_ROLL_TIMING_MAX_MS)
                config->dice_roll_lock_ms = SDL_DICE_ROLL_TIMING_MAX_MS;
            log_debug("Loaded diceRollLockMs: %d",
                config->dice_roll_lock_ms);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "diceRollOverlayMs");
        if (cJSON_IsNumber(item)) {
            config->dice_roll_overlay_ms = item->valueint;
            if (config->dice_roll_overlay_ms < 0)
                config->dice_roll_overlay_ms = 0;
            if (config->dice_roll_overlay_ms > SDL_DICE_ROLL_TIMING_MAX_MS)
                config->dice_roll_overlay_ms = SDL_DICE_ROLL_TIMING_MAX_MS;
            log_debug("Loaded diceRollOverlayMs: %d",
                config->dice_roll_overlay_ms);
        }
        
        // Window position and size for windowed mode
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowX");
        if (cJSON_IsNumber(item)) {
            config->window_x = item->valueint;
            log_debug("Loaded windowX: %d", config->window_x);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowY");
        if (cJSON_IsNumber(item)) {
            config->window_y = item->valueint;
            log_debug("Loaded windowY: %d", config->window_y);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowWidth");
        if (cJSON_IsNumber(item)) {
            config->window_width = item->valueint;
            log_debug("Loaded windowWidth: %d", config->window_width);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowHeight");
        if (cJSON_IsNumber(item)) {
            config->window_height = item->valueint;
            log_debug("Loaded windowHeight: %d", config->window_height);
        }
        
        // Custom fonts
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyFont");
        if (cJSON_IsString(item)) {
            SDL_strlcpy(config->story_font, item->valuestring, sizeof(config->story_font));
            log_debug("Loaded storyFont: %s", config->story_font);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyFont2");
        if (cJSON_IsString(item)) {
            SDL_strlcpy(config->story_font2, item->valuestring, sizeof(config->story_font2));
            log_debug("Loaded storyFont2: %s", config->story_font2);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "monospaceFont");
        if (cJSON_IsString(item)) {
            SDL_strlcpy(config->monospace_font, item->valuestring, sizeof(config->monospace_font));
            log_debug("Loaded monospaceFont: %s", config->monospace_font);
        }
        
        // Monospace font rendering options (with backward compatibility)
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoBold");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontBold");
        if (cJSON_IsBool(item)) {
            config->mono_bold = cJSON_IsTrue(item);
            log_debug("Loaded monoBold: %s", config->mono_bold ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoItalic");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontItalic");
        if (cJSON_IsBool(item)) {
            config->mono_italic = cJSON_IsTrue(item);
            log_debug("Loaded monoItalic: %s", config->mono_italic ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoUnderline");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontUnderline");
        if (cJSON_IsBool(item)) {
            config->mono_underline = cJSON_IsTrue(item);
            log_debug("Loaded monoUnderline: %s", config->mono_underline ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoStrikethrough");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontStrikethrough");
        if (cJSON_IsBool(item)) {
            config->mono_strikethrough = cJSON_IsTrue(item);
            log_debug("Loaded monoStrikethrough: %s", config->mono_strikethrough ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoHinting");
        if (!cJSON_IsNumber(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontHinting");
        if (cJSON_IsNumber(item)) {
            config->mono_hinting = item->valueint;
            log_debug("Loaded monoHinting: %d", config->mono_hinting);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoKerning");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontKerning");
        if (cJSON_IsBool(item)) {
            config->mono_kerning = cJSON_IsTrue(item);
            log_debug("Loaded monoKerning: %s", config->mono_kerning ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoOutline");
        if (!cJSON_IsNumber(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontOutline");
        if (cJSON_IsNumber(item)) {
            config->mono_outline = item->valueint;
            log_debug("Loaded monoOutline: %d", config->mono_outline);
        }
        
        // Story font rendering options
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyBold");
        if (cJSON_IsBool(item)) {
            config->story_bold = cJSON_IsTrue(item);
            log_debug("Loaded storyBold: %s", config->story_bold ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyItalic");
        if (cJSON_IsBool(item)) {
            config->story_italic = cJSON_IsTrue(item);
            log_debug("Loaded storyItalic: %s", config->story_italic ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyUnderline");
        if (cJSON_IsBool(item)) {
            config->story_underline = cJSON_IsTrue(item);
            log_debug("Loaded storyUnderline: %s", config->story_underline ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyStrikethrough");
        if (cJSON_IsBool(item)) {
            config->story_strikethrough = cJSON_IsTrue(item);
            log_debug("Loaded storyStrikethrough: %s", config->story_strikethrough ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyHinting");
        if (cJSON_IsNumber(item)) {
            config->story_hinting = item->valueint;
            log_debug("Loaded storyHinting: %d", config->story_hinting);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyKerning");
        if (cJSON_IsBool(item)) {
            config->story_kerning = cJSON_IsTrue(item);
            log_debug("Loaded storyKerning: %s", config->story_kerning ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyOutline");
        if (cJSON_IsNumber(item)) {
            config->story_outline = item->valueint;
            log_debug("Loaded storyOutline: %d", config->story_outline);
        }

        // Second story font rendering options
        item = cJSON_GetObjectItemCaseSensitive(sdl, "story2Bold");
        if (cJSON_IsBool(item)) {
            config->story2_bold = cJSON_IsTrue(item);
            log_debug("Loaded story2Bold: %s", config->story2_bold ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "story2Italic");
        if (cJSON_IsBool(item)) {
            config->story2_italic = cJSON_IsTrue(item);
            log_debug("Loaded story2Italic: %s", config->story2_italic ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "story2Underline");
        if (cJSON_IsBool(item)) {
            config->story2_underline = cJSON_IsTrue(item);
            log_debug("Loaded story2Underline: %s", config->story2_underline ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "story2Strikethrough");
        if (cJSON_IsBool(item)) {
            config->story2_strikethrough = cJSON_IsTrue(item);
            log_debug("Loaded story2Strikethrough: %s", config->story2_strikethrough ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "story2Hinting");
        if (cJSON_IsNumber(item)) {
            config->story2_hinting = item->valueint;
            log_debug("Loaded story2Hinting: %d", config->story2_hinting);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "story2Kerning");
        if (cJSON_IsBool(item)) {
            config->story2_kerning = cJSON_IsTrue(item);
            log_debug("Loaded story2Kerning: %s", config->story2_kerning ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "story2Outline");
        if (cJSON_IsNumber(item)) {
            config->story2_outline = item->valueint;
            log_debug("Loaded story2Outline: %d", config->story2_outline);
        }
    } else {
        log_warn("'sdl' object not found in JSON");
    }
    
    /* Parse legacy shared pane configuration first, then copy into all profiles.
     * If paneProfiles exists below, it overrides each mode separately. */
    sdl_config_load_pane_array(cJSON_GetObjectItemCaseSensitive(root, "panes"),
        legacy_panes, &legacy_pane_count, MAX_PANE_CONFIGS, "panes");
    sdl_config_init_pane_profiles_from_legacy(config, pane_profiles, profile_count,
        legacy_panes, legacy_pane_count);

    {
        cJSON* pane_profiles_obj = cJSON_GetObjectItemCaseSensitive(root, "paneProfiles");
        bool loaded_profiles[SDL_PANE_PROFILE_COUNT] = { false };

        if (cJSON_IsObject(pane_profiles_obj) && pane_profiles && profile_count > 0) {
            for (int mode = 0; mode < profile_count; mode++) {
                const char* mode_name = min_terminal_mode_to_string(mode);
                cJSON* profile_obj = cJSON_GetObjectItemCaseSensitive(pane_profiles_obj, mode_name);

                if (cJSON_IsObject(profile_obj)) {
                    sdl_config_load_pane_profile(profile_obj, &pane_profiles[mode], mode_name);
                    if (mode >= 0 && mode < SDL_PANE_PROFILE_COUNT)
                        loaded_profiles[mode] = true;
                }
            }

            if (profile_count > SDL_MIN_TERMINAL_COMPACT
                && loaded_profiles[SDL_MIN_TERMINAL_NORMAL]
                && !loaded_profiles[SDL_MIN_TERMINAL_COMPACT]) {
                sdl_config_copy_pane_profile(
                    &pane_profiles[SDL_MIN_TERMINAL_COMPACT],
                    &pane_profiles[SDL_MIN_TERMINAL_NORMAL]);
                log_info("paneProfiles.COMPACT missing; copied NORMAL profile into COMPACT");
            } else if (profile_count > SDL_MIN_TERMINAL_COMPACT
                && loaded_profiles[SDL_MIN_TERMINAL_COMPACT]
                && !loaded_profiles[SDL_MIN_TERMINAL_NORMAL]) {
                sdl_config_copy_pane_profile(
                    &pane_profiles[SDL_MIN_TERMINAL_NORMAL],
                    &pane_profiles[SDL_MIN_TERMINAL_COMPACT]);
                log_info("paneProfiles.NORMAL missing; copied COMPACT profile into NORMAL");
            }
        }
    }

    if (!saw_left_panel_expanded_on_launch && saw_enable_bottom_panes)
        sdl_config_migrate_default_log_pane(config, pane_profiles, profile_count);

    // Parse gamepad settings
    cJSON* gamepad = cJSON_GetObjectItemCaseSensitive(root, "gamepad");
    if (cJSON_IsObject(gamepad)) {
        cJSON* item;
        bool saw_shoulder_combo_binding = false;

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "enabled");
        if (cJSON_IsBool(item)) {
            config->gamepad_enabled = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.enabled: %s", config->gamepad_enabled ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "autoMode");
        if (cJSON_IsBool(item)) {
            config->gamepad_auto_mode = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.autoMode: %s", config->gamepad_auto_mode ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "steamdeckMode");
        if (cJSON_IsBool(item)) {
            config->steamdeck_mode = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.steamdeckMode: %s", config->steamdeck_mode ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "steamdeckInvEquipSameButtonCycle");
        if (cJSON_IsBool(item)) {
            config->steamdeck_inv_equip_same_button_cycle = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.steamdeckInvEquipSameButtonCycle: %s",
                config->steamdeck_inv_equip_same_button_cycle ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "useDpad");
        if (cJSON_IsBool(item)) {
            config->gamepad_use_dpad = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.useDpad: %s", config->gamepad_use_dpad ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "useLeftStick");
        if (cJSON_IsBool(item)) {
            config->gamepad_use_left_stick = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.useLeftStick: %s", config->gamepad_use_left_stick ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "deadzone");
        if (cJSON_IsNumber(item)) {
            config->gamepad_deadzone = item->valueint;
            log_debug("Loaded gamepad.deadzone: %d", config->gamepad_deadzone);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "triggerThreshold");
        if (cJSON_IsNumber(item)) {
            config->gamepad_trigger_threshold = item->valueint;
            log_debug("Loaded gamepad.triggerThreshold: %d", config->gamepad_trigger_threshold);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "buttonBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    config->gamepad_button_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.buttonBindings (%d entries)", count);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "triggerBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_TRIGGER_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    config->gamepad_trigger_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.triggerBindings (%d entries)", count);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "leftStickBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    config->gamepad_left_stick_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.leftStickBindings (%d entries)", count);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "rightStickBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    config->gamepad_right_stick_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.rightStickBindings (%d entries)", count);
        }

        for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_button_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    config->gamepad_button_combo_bindings[modifier],
                    SDL_GAMEPAD_BUTTON_COUNT);
            }

            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_trigger_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    config->gamepad_trigger_combo_bindings[modifier],
                    GAMEPAD_TRIGGER_COUNT);
            }

            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_left_stick_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    config->gamepad_left_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
            }

            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_right_stick_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    config->gamepad_right_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
            }
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "shoulderComboBinding");
        if (cJSON_IsNumber(item)) {
            saw_shoulder_combo_binding = true;
            config->gamepad_shoulder_combo_binding = item->valueint;
            log_debug("Loaded gamepad.shoulderComboBinding: %d", config->gamepad_shoulder_combo_binding);
        }

        if (!saw_shoulder_combo_binding
            && config->gamepad_shoulder_combo_binding == 'l'
            && sdl_config_gamepad_action_binding_count(config, 'l') > 1) {
            log_info("Legacy gamepad config already binds 'l'; clearing inherited shoulder combo binding");
            config->gamepad_shoulder_combo_binding = GAMEPAD_BIND_NONE;
        }

        if (sdl_config_should_upgrade_legacy_gamepad_defaults(config)) {
            log_info("Upgrading legacy default gamepad config to current defaults");
            sdl_config_set_default_gamepad_bindings(config);
            config->steamdeck_inv_equip_same_button_cycle = true;
        }

        if (config->gamepad_use_dpad) {
            config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_UP] = GAMEPAD_BIND_NONE;
            config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_DOWN] = GAMEPAD_BIND_NONE;
            config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = GAMEPAD_BIND_NONE;
            config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = GAMEPAD_BIND_NONE;
        }

        if (config->gamepad_use_left_stick) {
            for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
                config->gamepad_left_stick_bindings[i] = GAMEPAD_BIND_NONE;
            }
        }
    } else {
        log_warn("'gamepad' object not found in JSON");
    }

    {
        cJSON* legacy_swipe_enabled = NULL;
        cJSON* legacy_swipe_bindings = NULL;
        bool saw_touch_control_swipe_enabled = false;
        bool saw_touch_control_swipe_bindings = false;
        bool saw_touch_control_top_panel_mode = false;
        bool saw_touch_control_top_panel_button_count = false;
        int top_panel_bindings_count = -1;
        int top_panel_long_bindings_count = -1;
        cJSON* touch_pane = cJSON_GetObjectItemCaseSensitive(root, "touchPane");
        if (cJSON_IsObject(touch_pane)) {
            cJSON* bindings = cJSON_GetObjectItemCaseSensitive(touch_pane, "bindings");
            cJSON* labels = cJSON_GetObjectItemCaseSensitive(touch_pane, "labels");
            cJSON* second_bindings = cJSON_GetObjectItemCaseSensitive(touch_pane, "secondBindings");
            cJSON* second_labels = cJSON_GetObjectItemCaseSensitive(touch_pane, "secondLabels");
            cJSON* panel_names = cJSON_GetObjectItemCaseSensitive(touch_pane, "panelNames");
            cJSON* show_key_labels = cJSON_GetObjectItemCaseSensitive(touch_pane, "showKeyLabels");
            cJSON* inv_equip_cycle = cJSON_GetObjectItemCaseSensitive(touch_pane,
                "inventoryEquipmentCycle");
            bool old_touch_pane_defaults = !cJSON_IsBool(show_key_labels);
            legacy_swipe_enabled = cJSON_GetObjectItemCaseSensitive(touch_pane, "swipeEnabled");
            legacy_swipe_bindings = cJSON_GetObjectItemCaseSensitive(touch_pane, "swipeBindings");
            if (cJSON_IsArray(bindings)) {
                int count = cJSON_GetArraySize(bindings);
                if (count == 21) {
                    for (int i = 0; i < count && (i + 3) < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
                        cJSON* binding = cJSON_GetArrayItem(bindings, i);
                        if (cJSON_IsNumber(binding)) {
                            int value = binding->valueint;
                            if (value == ' ')
                                value = INPUT_BIND_CONFIRM;
                            config->touch_pane_bindings[i + 3] = value;
                        }
                    }
                    log_info("Migrated legacy touchPane.bindings layout (21 -> %d entries)",
                        SDL_TOUCH_PANE_BUTTON_COUNT);
                } else {
                    sdl_config_load_touch_binding_array(bindings, config->touch_pane_bindings,
                        SDL_TOUCH_PANE_BUTTON_COUNT);
                }
                log_debug("Loaded touchPane.bindings (%d entries)", count);
            }

            if (cJSON_IsArray(labels)) {
                int count = cJSON_GetArraySize(labels);
                sdl_config_load_touch_label_array(labels, config->touch_pane_labels,
                    SDL_TOUCH_PANE_BUTTON_COUNT);
                log_debug("Loaded touchPane.labels (%d entries)", count);
            }

            if (cJSON_IsArray(second_bindings)) {
                int count = cJSON_GetArraySize(second_bindings);
                sdl_config_load_touch_binding_array(second_bindings, config->touch_pane_second_bindings,
                    SDL_TOUCH_PANE_BUTTON_COUNT);
                log_debug("Loaded touchPane.secondBindings (%d entries)", count);
            }

            if (cJSON_IsArray(second_labels)) {
                int count = cJSON_GetArraySize(second_labels);
                sdl_config_load_touch_label_array(second_labels, config->touch_pane_second_labels,
                    SDL_TOUCH_PANE_BUTTON_COUNT);
                log_debug("Loaded touchPane.secondLabels (%d entries)", count);
            }

            if (cJSON_IsArray(panel_names)) {
                int count = cJSON_GetArraySize(panel_names);
                for (int i = 0; i < SDL_TOUCH_PANE_PANEL_COUNT && i < count; i++) {
                    cJSON* panel_name = cJSON_GetArrayItem(panel_names, i);
                    if (cJSON_IsString(panel_name) && panel_name->valuestring) {
                        SDL_strlcpy(config->touch_pane_panel_names[i], panel_name->valuestring,
                            sizeof(config->touch_pane_panel_names[i]));
                    }
                }
                log_debug("Loaded touchPane.panelNames (%d entries)", count);
            }

            if (cJSON_IsBool(show_key_labels)) {
                config->touch_pane_key_labels_visible =
                    cJSON_IsTrue(show_key_labels);
                log_debug("Loaded touchPane.showKeyLabels: %s",
                    config->touch_pane_key_labels_visible ? "true" : "false");
            }

            if (cJSON_IsBool(inv_equip_cycle)) {
                config->touch_pane_inventory_equipment_cycle =
                    cJSON_IsTrue(inv_equip_cycle);
                log_debug("Loaded touchPane.inventoryEquipmentCycle: %s",
                    config->touch_pane_inventory_equipment_cycle ? "true" : "false");
            }

            if (old_touch_pane_defaults)
                sdl_config_migrate_touch_pane_binding(config,
                    SDL_TOUCH_PANE_PANEL_MAIN, 1, GAMEPAD_BIND_CTRL, 'S',
                    "Migrated default touch pane Ctrl button to Stealth");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 0, TOUCH_PANE_BIND_INHERIT,
                GAMEPAD_BIND_CTRL,
                "Migrated default touch pane Esc second-panel button to Ctrl");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_MAIN, 3, 'e', 'h',
                "Migrated default touch pane Equip button to Char");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_MAIN, 5, '-', 'j',
                "Migrated default touch pane Fletch button to Supply");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 1, TOUCH_PANE_BIND_INHERIT, 'X',
                "Migrated default touch pane Stealth second-panel button to Exchange");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 3, '0', '\t',
                "Migrated default touch pane Char second-panel button to Ability");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 4, '-', 'e',
                "Migrated default touch pane Inv second-panel button to Equip");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 7, 'S', '0',
                "Migrated default touch pane Sing second-panel button to Smith");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 5, 'q', '-',
                "Migrated default touch pane Supply second-panel button to Fletch");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 18, 'L', 'M',
                "Migrated default touch pane View second-panel button to Map");
            sdl_config_migrate_touch_pane_binding(config,
                SDL_TOUCH_PANE_PANEL_SECOND, 19, 'X', 'q',
                "Migrated default touch pane Desc second-panel button to Quaff");

            if (old_touch_pane_defaults
                && strcmp(config->touch_pane_panel_names[
                    SDL_TOUCH_PANE_PANEL_SECOND], "Shift") == 0)
            {
                SDL_strlcpy(config->touch_pane_panel_names[
                        SDL_TOUCH_PANE_PANEL_SECOND], "2nd Panel",
                    sizeof(config->touch_pane_panel_names[
                        SDL_TOUCH_PANE_PANEL_SECOND]));
                log_info("Migrated default touch pane Shift panel name to 2nd Panel");
            }
        } else {
            log_warn("'touchPane' object not found in JSON");
        }

        {
            cJSON* touch_control = cJSON_GetObjectItemCaseSensitive(root, "touchControl");
            if (cJSON_IsObject(touch_control)) {
                cJSON* profile = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "profile");
                cJSON* touch_pane_default_open = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "touchPaneDefaultOpen");
                cJSON* menu_commands = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "menuCommandsEnabled");
                cJSON* inventory_menu_commands = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "inventoryEquipmentMenuCommandsEnabled");
                cJSON* supply_menu_commands = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "supplyMenuCommandsEnabled");
                cJSON* other_menu_commands = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "otherMenuCommandsEnabled");
                cJSON* movement_mode = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "movementMode");
                cJSON* round_movement = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "roundMovementLayerEnabled");
                cJSON* corner_button_overlay = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "cornerButtonOverlayMode");
                cJSON* corner_button_markers = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "cornerButtonMarkersEnabled");
                cJSON* corner_button_borders = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "cornerButtonBordersEnabled");
                cJSON* corner_button_center_bindings = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "cornerButtonCenterBindings");
                cJSON* corner_button_up_down_side = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "cornerButtonUpDownSide");
                cJSON* corner_button_action_bindings = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "cornerButtonActionBindings");
                cJSON* top_panel_mode = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "topPanelMode");
                cJSON* top_panel_default_open = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "topPanelDefaultOpen");
                cJSON* top_panel_button_count = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "topPanelButtonCount");
                cJSON* top_panel_tile_scale = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "topPanelTileScale");
                cJSON* top_panel_bindings = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "topPanelBindings");
                cJSON* top_panel_long_bindings = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "topPanelLongBindings");
                cJSON* thumb_enabled = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "thumbButtonsEnabled");
                cJSON* thumb_bindings = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "thumbButtonBindings");
                cJSON* thumb_long_bindings = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "thumbButtonLongBindings");
                cJSON* swipe_enabled = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "swipeEnabled");
                cJSON* swipe_bindings = cJSON_GetObjectItemCaseSensitive(touch_control,
                    "swipeBindings");

                if (cJSON_IsString(profile) && profile->valuestring) {
                    config->touch_profile =
                        parse_touch_profile(profile->valuestring);
                    log_debug("Loaded touchControl.profile: %s",
                        touch_profile_to_string(config->touch_profile));
                } else if (cJSON_IsNumber(profile)) {
                    config->touch_profile =
                        normalize_touch_profile(profile->valueint);
                    log_debug("Loaded numeric touchControl.profile: %s",
                        touch_profile_to_string(config->touch_profile));
                }

                if (cJSON_IsBool(touch_pane_default_open)) {
                    config->touch_pane_default_open =
                        cJSON_IsTrue(touch_pane_default_open);
                    log_debug("Loaded touchControl.touchPaneDefaultOpen: %s",
                        config->touch_pane_default_open ? "true" : "false");
                }

                if (cJSON_IsBool(menu_commands)) {
                    bool value = cJSON_IsTrue(menu_commands);

                    for (int i = 0; i < SDL_TOUCH_MENU_CATEGORY_COUNT; i++)
                        config->touch_menu_command_enabled[i] = value;
                    log_debug("Loaded legacy touchControl.menuCommandsEnabled: %s",
                        value ? "true" : "false");
                }

                if (cJSON_IsBool(inventory_menu_commands)) {
                    config->touch_menu_command_enabled[
                        SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT]
                        = cJSON_IsTrue(inventory_menu_commands);
                    log_debug("Loaded touchControl.inventoryEquipmentMenuCommandsEnabled: %s",
                        config->touch_menu_command_enabled[
                            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT] ? "true" : "false");
                }

                if (cJSON_IsBool(supply_menu_commands)) {
                    config->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_SUPPLY]
                        = cJSON_IsTrue(supply_menu_commands);
                    log_debug("Loaded touchControl.supplyMenuCommandsEnabled: %s",
                        config->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_SUPPLY]
                            ? "true" : "false");
                }

                if (cJSON_IsBool(other_menu_commands)) {
                    config->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_OTHER]
                        = cJSON_IsTrue(other_menu_commands);
                    log_debug("Loaded touchControl.otherMenuCommandsEnabled: %s",
                        config->touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_OTHER]
                            ? "true" : "false");
                }

                if (cJSON_IsString(movement_mode) && movement_mode->valuestring) {
                    config->touch_movement_mode =
                        parse_touch_movement_mode(movement_mode->valuestring);
                    log_debug("Loaded touchControl.movementMode: %s",
                        touch_movement_mode_to_string(config->touch_movement_mode));
                } else if (cJSON_IsNumber(movement_mode)) {
                    config->touch_movement_mode =
                        normalize_touch_movement_mode(movement_mode->valueint);
                    log_debug("Loaded numeric touchControl.movementMode: %s",
                        touch_movement_mode_to_string(config->touch_movement_mode));
                }

                if (cJSON_IsBool(round_movement)) {
                    config->touch_round_movement_enabled =
                        cJSON_IsTrue(round_movement);
                    log_debug("Loaded touchControl.roundMovementLayerEnabled: %s",
                        config->touch_round_movement_enabled ? "true" : "false");
                }

                if (cJSON_IsString(corner_button_overlay)
                    && corner_button_overlay->valuestring)
                {
                    config->touch_zone_overlay_mode =
                        parse_touch_zone_overlay_mode(
                            corner_button_overlay->valuestring);
                    log_debug("Loaded touchControl.cornerButtonOverlayMode: %s",
                        touch_zone_overlay_mode_to_string(
                            config->touch_zone_overlay_mode));
                } else if (cJSON_IsNumber(corner_button_overlay)) {
                    config->touch_zone_overlay_mode =
                        normalize_touch_zone_overlay_mode(
                            corner_button_overlay->valueint);
                    log_debug("Loaded numeric touchControl.cornerButtonOverlayMode: %s",
                        touch_zone_overlay_mode_to_string(
                            config->touch_zone_overlay_mode));
                } else if (cJSON_IsBool(corner_button_borders)
                    || cJSON_IsBool(corner_button_markers))
                {
                    bool borders = cJSON_IsBool(corner_button_borders)
                        && cJSON_IsTrue(corner_button_borders);
                    bool markers = !cJSON_IsBool(corner_button_markers)
                        || cJSON_IsTrue(corner_button_markers);

                    config->touch_zone_overlay_mode = borders
                        ? SDL_TOUCH_ZONE_OVERLAY_BORDERS
                        : (markers ? SDL_TOUCH_ZONE_OVERLAY_MARKERS
                                   : SDL_TOUCH_ZONE_OVERLAY_OFF);
                    log_debug("Migrated touchControl corner button overlay mode: %s",
                        touch_zone_overlay_mode_to_string(
                            config->touch_zone_overlay_mode));
                }

                if (cJSON_IsArray(corner_button_center_bindings)) {
                    int count = cJSON_GetArraySize(corner_button_center_bindings);
                    sdl_config_load_touch_binding_array(
                        corner_button_center_bindings,
                        config->touch_zone_center_bindings,
                        SDL_TOUCH_ZONE_CENTER_BINDING_COUNT);
                    log_debug("Loaded touchControl.cornerButtonCenterBindings (%d entries)",
                        count);
                }

                if (cJSON_IsString(corner_button_up_down_side)
                    && corner_button_up_down_side->valuestring)
                {
                    config->touch_corner_up_down_side =
                        parse_touch_corner_up_down_side(
                            corner_button_up_down_side->valuestring);
                    log_debug("Loaded touchControl.cornerButtonUpDownSide: %s",
                        touch_corner_up_down_side_to_string(
                            config->touch_corner_up_down_side));
                } else if (cJSON_IsNumber(corner_button_up_down_side)) {
                    config->touch_corner_up_down_side =
                        normalize_touch_corner_up_down_side(
                            corner_button_up_down_side->valueint);
                    log_debug("Loaded numeric touchControl.cornerButtonUpDownSide: %s",
                        touch_corner_up_down_side_to_string(
                            config->touch_corner_up_down_side));
                }

                if (cJSON_IsArray(corner_button_action_bindings)) {
                    int count = cJSON_GetArraySize(corner_button_action_bindings);
                    sdl_config_load_touch_binding_array(
                        corner_button_action_bindings,
                        config->touch_corner_action_bindings,
                        SDL_TOUCH_CORNER_ACTION_BINDING_COUNT);
                    log_debug("Loaded touchControl.cornerButtonActionBindings (%d entries)",
                        count);
                }

                if (cJSON_IsString(top_panel_mode)
                    && top_panel_mode->valuestring)
                {
                    saw_touch_control_top_panel_mode = true;
                    config->touch_top_panel_mode =
                        parse_touch_top_panel_mode(
                            top_panel_mode->valuestring);
                    log_debug("Loaded touchControl.topPanelMode: %s",
                        touch_top_panel_mode_to_string(
                            config->touch_top_panel_mode));
                } else if (cJSON_IsNumber(top_panel_mode)) {
                    saw_touch_control_top_panel_mode = true;
                    config->touch_top_panel_mode =
                        normalize_touch_top_panel_mode(
                            top_panel_mode->valueint);
                    log_debug("Loaded numeric touchControl.topPanelMode: %s",
                        touch_top_panel_mode_to_string(
                            config->touch_top_panel_mode));
                }

                if (cJSON_IsBool(top_panel_default_open)) {
                    config->touch_top_panel_default_open =
                        cJSON_IsTrue(top_panel_default_open);
                    log_debug("Loaded touchControl.topPanelDefaultOpen: %s",
                        config->touch_top_panel_default_open ? "true" : "false");
                }

                if (cJSON_IsNumber(top_panel_button_count)) {
                    saw_touch_control_top_panel_button_count = true;
                    config->touch_top_panel_button_count =
                        normalize_touch_top_panel_button_count(
                            top_panel_button_count->valueint);
                    log_debug("Loaded touchControl.topPanelButtonCount: %d",
                        config->touch_top_panel_button_count);
                }

                if (cJSON_IsNumber(top_panel_tile_scale)) {
                    config->touch_top_panel_tile_scale =
                        normalize_touch_top_panel_tile_scale(
                            top_panel_tile_scale->valueint);
                    log_debug("Loaded touchControl.topPanelTileScale: %d",
                        config->touch_top_panel_tile_scale);
                }

                if (cJSON_IsArray(top_panel_bindings)) {
                    int count = cJSON_GetArraySize(top_panel_bindings);
                    top_panel_bindings_count = count;
                    sdl_config_load_touch_binding_array(
                        top_panel_bindings,
                        config->touch_top_panel_bindings,
                        SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
                    log_debug("Loaded touchControl.topPanelBindings (%d entries)",
                        count);
                }

                if (cJSON_IsArray(top_panel_long_bindings)) {
                    int count = cJSON_GetArraySize(top_panel_long_bindings);
                    top_panel_long_bindings_count = count;
                    sdl_config_load_touch_binding_array(
                        top_panel_long_bindings,
                        config->touch_top_panel_long_bindings,
                        SDL_TOUCH_TOP_PANEL_BUTTON_COUNT);
                    log_debug("Loaded touchControl.topPanelLongBindings (%d entries)",
                        count);
                }

                if (cJSON_IsBool(thumb_enabled)) {
                    config->touch_thumb_enabled = cJSON_IsTrue(thumb_enabled);
                    log_debug("Loaded touchControl.thumbButtonsEnabled: %s",
                        config->touch_thumb_enabled ? "true" : "false");
                }

                if (cJSON_IsArray(thumb_bindings)) {
                    int count = cJSON_GetArraySize(thumb_bindings);
                    sdl_config_load_touch_binding_array(
                        thumb_bindings,
                        config->touch_thumb_bindings,
                        SDL_TOUCH_THUMB_BUTTON_COUNT);
                    log_debug("Loaded touchControl.thumbButtonBindings (%d entries)",
                        count);
                }

                if (cJSON_IsArray(thumb_long_bindings)) {
                    int count = cJSON_GetArraySize(thumb_long_bindings);
                    sdl_config_load_touch_binding_array(
                        thumb_long_bindings,
                        config->touch_thumb_long_bindings,
                        SDL_TOUCH_THUMB_BUTTON_COUNT);
                    log_debug("Loaded touchControl.thumbButtonLongBindings (%d entries)",
                        count);
                }

                if (cJSON_IsBool(swipe_enabled)) {
                    saw_touch_control_swipe_enabled = true;
                    config->touch_swipe_enabled = cJSON_IsTrue(swipe_enabled);
                    log_debug("Loaded touchControl.swipeEnabled: %s",
                        config->touch_swipe_enabled ? "true" : "false");
                }

                if (cJSON_IsArray(swipe_bindings)) {
                    int count = cJSON_GetArraySize(swipe_bindings);
                    saw_touch_control_swipe_bindings = true;
                    sdl_config_load_touch_binding_array(swipe_bindings,
                        config->touch_swipe_bindings, TOUCH_SWIPE_DIR_COUNT);
                    log_debug("Loaded touchControl.swipeBindings (%d entries)", count);
                }
            }
        }

        sdl_config_migrate_touch_top_panel_layout(config,
            top_panel_bindings_count, top_panel_long_bindings_count);
        sdl_config_migrate_touch_top_panel_defaults(config);
        if (!saw_touch_control_top_panel_mode) {
            config->touch_top_panel_mode = SDL_TOUCH_TOP_PANEL_MODE_SHORT;
        }
        if (!saw_touch_control_top_panel_button_count) {
            config->touch_top_panel_button_count =
                (config->touch_top_panel_mode == SDL_TOUCH_TOP_PANEL_MODE_LONG)
                    ? SDL_TOUCH_TOP_PANEL_BUTTON_COUNT
                    : SDL_TOUCH_TOP_PANEL_BUTTON_COUNT_DEFAULT;
        }

        if (!saw_touch_control_swipe_enabled && cJSON_IsBool(legacy_swipe_enabled)) {
            config->touch_swipe_enabled = cJSON_IsTrue(legacy_swipe_enabled);
            log_debug("Loaded legacy touchPane.swipeEnabled: %s",
                config->touch_swipe_enabled ? "true" : "false");
        }

        if (!saw_touch_control_swipe_bindings && cJSON_IsArray(legacy_swipe_bindings)) {
            int count = cJSON_GetArraySize(legacy_swipe_bindings);
            sdl_config_load_touch_binding_array(legacy_swipe_bindings,
                config->touch_swipe_bindings, TOUCH_SWIPE_DIR_COUNT);
            log_debug("Loaded legacy touchPane.swipeBindings (%d entries)", count);
        }

        if (config->touch_swipe_bindings[TOUCH_SWIPE_DIR_UP] == '8'
            && config->touch_swipe_bindings[TOUCH_SWIPE_DIR_DOWN] == '2'
            && config->touch_swipe_bindings[TOUCH_SWIPE_DIR_LEFT] == '4'
            && config->touch_swipe_bindings[TOUCH_SWIPE_DIR_RIGHT] == '6')
        {
            config->touch_swipe_bindings[TOUCH_SWIPE_DIR_UP] =
                TOUCH_BIND_TOP_PANEL_CLOSE;
            config->touch_swipe_bindings[TOUCH_SWIPE_DIR_DOWN] =
                TOUCH_BIND_TOP_PANEL_OPEN;
            log_info("Migrated default touch swipe up/down bindings to quick access actions");
        }

        config->touch_movement_mode =
            normalize_touch_movement_mode(config->touch_movement_mode);
        config->touch_zone_overlay_mode =
            normalize_touch_zone_overlay_mode(config->touch_zone_overlay_mode);
        config->touch_corner_up_down_side =
            normalize_touch_corner_up_down_side(config->touch_corner_up_down_side);
        config->touch_top_panel_mode =
            normalize_touch_top_panel_mode(config->touch_top_panel_mode);
        config->touch_top_panel_tile_scale =
            normalize_touch_top_panel_tile_scale(config->touch_top_panel_tile_scale);
        config->touch_top_panel_button_count =
            normalize_touch_top_panel_button_count(
                config->touch_top_panel_button_count);
        config->touch_profile =
            normalize_touch_profile(config->touch_profile);
    }

    {
        cJSON* mouse_control = cJSON_GetObjectItemCaseSensitive(root, "mouseControl");
        if (cJSON_IsObject(mouse_control)) {
            cJSON* enabled = cJSON_GetObjectItemCaseSensitive(mouse_control,
                "enabled");
            cJSON* movement_mode = cJSON_GetObjectItemCaseSensitive(mouse_control,
                "movementMode");

            if (cJSON_IsBool(enabled)) {
                config->mouse_enabled = cJSON_IsTrue(enabled);
                log_debug("Loaded mouseControl.enabled: %s",
                    config->mouse_enabled ? "true" : "false");
            }

            if (cJSON_IsString(movement_mode) && movement_mode->valuestring) {
                config->mouse_movement_mode =
                    parse_mouse_movement_mode(movement_mode->valuestring);
                log_debug("Loaded mouseControl.movementMode: %s",
                    mouse_movement_mode_to_string(config->mouse_movement_mode));
            } else if (cJSON_IsNumber(movement_mode)) {
                config->mouse_movement_mode =
                    normalize_mouse_movement_mode(movement_mode->valueint);
                log_debug("Loaded numeric mouseControl.movementMode: %s",
                    mouse_movement_mode_to_string(config->mouse_movement_mode));
            }
        }

        config->mouse_movement_mode =
            normalize_mouse_movement_mode(config->mouse_movement_mode);
    }

    cJSON_Delete(root);
    log_debug("Configuration loading complete. Active mode=%s", min_terminal_mode_to_string(config->min_terminal_mode));
    return SDL_CONFIG_LOAD_OK;
}

void sdl_config_save(const char* filename, const struct sdl_config* config,
                     const struct sdl_pane_profile* pane_profiles, int profile_count)
{
    int active_mode = config->min_terminal_mode;
    const struct sdl_pane_profile* active_profile = NULL;
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        log_error("Failed to create JSON root object");
        return;
    }
    
    // Create SDL settings object
    cJSON* sdl = cJSON_CreateObject();
    if (!sdl) {
        cJSON_Delete(root);
        log_error("Failed to create SDL settings object");
        return;
    }
    
    cJSON_AddNumberToObject(sdl, "mainViewScale", config->main_view_scale);
    cJSON_AddNumberToObject(sdl, "auxViewFontSize", config->aux_view_font_size);
    cJSON_AddNumberToObject(sdl, "margin", config->margin);
    cJSON_AddBoolToObject(sdl, "fullscreen", config->fullscreen);
    cJSON_AddBoolToObject(sdl, "tiles", config->tiles);
    cJSON_AddStringToObject(sdl, "palettePreset", config->palette_preset);
    cJSON_AddBoolToObject(sdl, "useUnsafeArea", config->use_unsafe_area);
    cJSON_AddBoolToObject(sdl, "enableRightPanes", config->enable_right_panes);
    cJSON_AddBoolToObject(sdl, "enableBottomPanes", config->enable_bottom_panes);
    cJSON_AddBoolToObject(sdl, "showPaneBorders", config->show_pane_borders);
    cJSON_AddBoolToObject(sdl, "leftPanelExpandedOnLaunch",
        config->left_panel_expanded_on_launch);
    cJSON_AddStringToObject(sdl, "leftPanelCompactMode",
        left_panel_compact_mode_to_string(config->left_panel_compact_mode));
    cJSON_AddStringToObject(sdl, "minTerminalMode", min_terminal_mode_to_string(config->min_terminal_mode));
    cJSON_AddNumberToObject(sdl, "logPaneDisplayFilter",
        config->log_pane_display_filter);
    cJSON_AddNumberToObject(sdl, "diceRollLockMs", config->dice_roll_lock_ms);
    cJSON_AddNumberToObject(sdl, "diceRollOverlayMs",
        config->dice_roll_overlay_ms);
    
    // Save window position and size for windowed mode
    cJSON_AddNumberToObject(sdl, "windowX", config->window_x);
    cJSON_AddNumberToObject(sdl, "windowY", config->window_y);
    cJSON_AddNumberToObject(sdl, "windowWidth", config->window_width);
    cJSON_AddNumberToObject(sdl, "windowHeight", config->window_height);
    
    // Save custom fonts
    cJSON_AddStringToObject(sdl, "storyFont", config->story_font);
    cJSON_AddStringToObject(sdl, "storyFont2", config->story_font2);
    cJSON_AddStringToObject(sdl, "monospaceFont", config->monospace_font);
    
    // Save monospace font rendering options
    cJSON_AddBoolToObject(sdl, "monoBold", config->mono_bold);
    cJSON_AddBoolToObject(sdl, "monoItalic", config->mono_italic);
    cJSON_AddBoolToObject(sdl, "monoUnderline", config->mono_underline);
    cJSON_AddBoolToObject(sdl, "monoStrikethrough", config->mono_strikethrough);
    cJSON_AddNumberToObject(sdl, "monoHinting", config->mono_hinting);
    cJSON_AddBoolToObject(sdl, "monoKerning", config->mono_kerning);
    cJSON_AddNumberToObject(sdl, "monoOutline", config->mono_outline);
    
    // Save story font rendering options
    cJSON_AddBoolToObject(sdl, "storyBold", config->story_bold);
    cJSON_AddBoolToObject(sdl, "storyItalic", config->story_italic);
    cJSON_AddBoolToObject(sdl, "storyUnderline", config->story_underline);
    cJSON_AddBoolToObject(sdl, "storyStrikethrough", config->story_strikethrough);
    cJSON_AddNumberToObject(sdl, "storyHinting", config->story_hinting);
    cJSON_AddBoolToObject(sdl, "storyKerning", config->story_kerning);
    cJSON_AddNumberToObject(sdl, "storyOutline", config->story_outline);

    // Save second story font rendering options
    cJSON_AddBoolToObject(sdl, "story2Bold", config->story2_bold);
    cJSON_AddBoolToObject(sdl, "story2Italic", config->story2_italic);
    cJSON_AddBoolToObject(sdl, "story2Underline", config->story2_underline);
    cJSON_AddBoolToObject(sdl, "story2Strikethrough", config->story2_strikethrough);
    cJSON_AddNumberToObject(sdl, "story2Hinting", config->story2_hinting);
    cJSON_AddBoolToObject(sdl, "story2Kerning", config->story2_kerning);
    cJSON_AddNumberToObject(sdl, "story2Outline", config->story2_outline);

    cJSON_AddItemToObject(root, "sdl", sdl);

    {
        cJSON* keyboard = sdl_config_create_keyboard_keymaps();
        if (keyboard)
            cJSON_AddItemToObject(root, "keyboard", keyboard);
    }

    sdl_config_save_movement_bindings(root, config);

    if (active_mode < 0 || active_mode >= profile_count)
        active_mode = SDL_MIN_TERMINAL_NORMAL;
    if (pane_profiles && active_mode >= 0 && active_mode < profile_count)
        active_profile = &pane_profiles[active_mode];

    {
        cJSON* panes = active_profile
            ? sdl_config_create_panes_array(active_profile->pane_configs, active_profile->pane_count)
            : cJSON_CreateArray();

        if (!panes) {
            cJSON_Delete(root);
            log_error("Failed to create panes array");
            return;
        }

        cJSON_AddItemToObject(root, "panes", panes);
    }

    {
        cJSON* pane_profiles_obj = cJSON_CreateObject();

        if (!pane_profiles_obj) {
            cJSON_Delete(root);
            log_error("Failed to create paneProfiles object");
            return;
        }

        for (int mode = 0; mode < profile_count; mode++) {
            cJSON* profile_obj = cJSON_CreateObject();
            cJSON* panes = NULL;

            if (!profile_obj)
                continue;

            cJSON_AddNumberToObject(profile_obj, "mainViewScale",
                pane_profiles[mode].main_view_scale);
            cJSON_AddNumberToObject(profile_obj, "auxViewFontSize",
                pane_profiles[mode].aux_view_font_size);
            cJSON_AddBoolToObject(profile_obj, "enableRightPanes",
                pane_profiles[mode].enable_right_panes);
            cJSON_AddBoolToObject(profile_obj, "enableBottomPanes",
                pane_profiles[mode].enable_bottom_panes);

            panes = sdl_config_create_panes_array(pane_profiles[mode].pane_configs,
                pane_profiles[mode].pane_count);
            if (!panes) {
                cJSON_Delete(profile_obj);
                continue;
            }

            cJSON_AddItemToObject(profile_obj, "panes", panes);
            cJSON_AddItemToObject(pane_profiles_obj,
                min_terminal_mode_to_string(mode), profile_obj);
        }

        cJSON_AddItemToObject(root, "paneProfiles", pane_profiles_obj);
    }

    // Create gamepad settings object
    {
        cJSON* gamepad = cJSON_CreateObject();
        if (gamepad) {
            cJSON* bindings = NULL;
            cJSON* triggers = NULL;
            cJSON* left_stick = NULL;
            cJSON* right_stick = NULL;

            cJSON_AddBoolToObject(gamepad, "enabled", config->gamepad_enabled);
            cJSON_AddBoolToObject(gamepad, "autoMode", config->gamepad_auto_mode);
            cJSON_AddBoolToObject(gamepad, "steamdeckMode", config->steamdeck_mode);
            cJSON_AddBoolToObject(gamepad, "steamdeckInvEquipSameButtonCycle",
                config->steamdeck_inv_equip_same_button_cycle);
            cJSON_AddBoolToObject(gamepad, "useDpad", config->gamepad_use_dpad);
            cJSON_AddBoolToObject(gamepad, "useLeftStick", config->gamepad_use_left_stick);
            cJSON_AddNumberToObject(gamepad, "deadzone", config->gamepad_deadzone);
            cJSON_AddNumberToObject(gamepad, "triggerThreshold", config->gamepad_trigger_threshold);

            bindings = cJSON_CreateArray();
            if (bindings) {
                for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
                    cJSON_AddItemToArray(bindings, cJSON_CreateNumber(config->gamepad_button_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "buttonBindings", bindings);
            }

            triggers = cJSON_CreateArray();
            if (triggers) {
                for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
                    cJSON_AddItemToArray(triggers, cJSON_CreateNumber(config->gamepad_trigger_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "triggerBindings", triggers);
            }

            left_stick = cJSON_CreateArray();
            if (left_stick) {
                for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
                    cJSON_AddItemToArray(left_stick, cJSON_CreateNumber(config->gamepad_left_stick_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "leftStickBindings", left_stick);
            }

            right_stick = cJSON_CreateArray();
            if (right_stick) {
                for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
                    cJSON_AddItemToArray(right_stick, cJSON_CreateNumber(config->gamepad_right_stick_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "rightStickBindings", right_stick);
            }

            for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
                cJSON* combo_array = sdl_config_create_int_array(
                    config->gamepad_button_combo_bindings[modifier],
                    SDL_GAMEPAD_BUTTON_COUNT);
                if (combo_array) {
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_button_combo_names[modifier],
                        combo_array);
                }

                combo_array = sdl_config_create_int_array(
                    config->gamepad_trigger_combo_bindings[modifier],
                    GAMEPAD_TRIGGER_COUNT);
                if (combo_array) {
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_trigger_combo_names[modifier],
                        combo_array);
                }

                combo_array = sdl_config_create_int_array(
                    config->gamepad_left_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
                if (combo_array) {
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_left_stick_combo_names[modifier],
                        combo_array);
                }

                combo_array = sdl_config_create_int_array(
                    config->gamepad_right_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
                if (combo_array) {
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_right_stick_combo_names[modifier],
                        combo_array);
                }
            }

            cJSON_AddNumberToObject(gamepad, "shoulderComboBinding", config->gamepad_shoulder_combo_binding);

            cJSON_AddItemToObject(root, "gamepad", gamepad);
        }
    }

    {
        cJSON* mouse_control = cJSON_CreateObject();
        if (mouse_control) {
            cJSON_AddBoolToObject(mouse_control, "enabled",
                config->mouse_enabled);
            cJSON_AddStringToObject(mouse_control, "movementMode",
                mouse_movement_mode_to_string(config->mouse_movement_mode));
            cJSON_AddItemToObject(root, "mouseControl", mouse_control);
        }
    }

    /*
     * Persist the thumb-button overlay settings. Other touchControl fields are
     * not (yet) serialized here, so this object intentionally carries only the
     * thumb keys; the loader reads each key independently and leaves the rest at
     * their defaults, so writing a partial object does not affect them.
     */
    {
        cJSON* touch_control = cJSON_CreateObject();
        if (touch_control) {
            cJSON* thumb_bindings = sdl_config_create_int_array(
                config->touch_thumb_bindings, SDL_TOUCH_THUMB_BUTTON_COUNT);
            cJSON* thumb_long_bindings = sdl_config_create_int_array(
                config->touch_thumb_long_bindings, SDL_TOUCH_THUMB_BUTTON_COUNT);

            cJSON_AddBoolToObject(touch_control, "thumbButtonsEnabled",
                config->touch_thumb_enabled);
            if (thumb_bindings)
                cJSON_AddItemToObject(touch_control, "thumbButtonBindings",
                    thumb_bindings);
            if (thumb_long_bindings)
                cJSON_AddItemToObject(touch_control, "thumbButtonLongBindings",
                    thumb_long_bindings);
            cJSON_AddItemToObject(root, "touchControl", touch_control);
        }
    }

    /* Create app-wide options object */
    {
        cJSON* app_options = cJSON_CreateObject();
        cJSON* interface = NULL;
        cJSON* visual = NULL;

        if (app_options && op_ptr) {
            cJSON_AddBoolToObject(app_options, "introSeen", g_app_intro_seen);
            cJSON_AddBoolToObject(app_options, "touchTutorialSeen",
                g_app_touch_tutorial_seen);
            cJSON_AddBoolToObject(app_options, "mouseTutorialSeen",
                g_app_mouse_tutorial_seen);
            cJSON_AddBoolToObject(app_options, "characterWheelTutorialSeen",
                g_app_character_wheel_tutorial_seen);
            cJSON_AddBoolToObject(app_options, "keyboardPresetPromptSeen",
                g_app_keyboard_preset_prompt_seen);

            sdl_config_save_app_option_group(app_options, "input", app_input_options);
            sdl_config_save_app_option_group(app_options, "interface", app_interface_options);
            sdl_config_save_app_option_group(app_options, "text", app_text_options);
            sdl_config_save_app_option_group(app_options, "gameplay", app_gameplay_options);
            sdl_config_save_app_option_group(app_options, "visual", app_visual_options);

            interface = cJSON_GetObjectItemCaseSensitive(app_options, "interface");
            if (cJSON_IsObject(interface)) {
                cJSON_AddNumberToObject(interface, "hitpointWarning", op_ptr->hitpoint_warn);
            }

            visual = cJSON_GetObjectItemCaseSensitive(app_options, "visual");
            if (cJSON_IsObject(visual)) {
                cJSON_AddNumberToObject(visual, "delayFactor",
                    op_ptr->delay_factor);
                cJSON_AddNumberToObject(visual, "runningDelayMs",
                    op_ptr->running_delay_ms);
                cJSON_AddNumberToObject(visual, "introStyle", op_ptr->intro_style);
                cJSON_AddNumberToObject(visual, "levelEntryNarrativeMode",
                    op_ptr->level_entry_narrative_mode);
                cJSON_AddNumberToObject(visual, "partitionNarrativeMode",
                    op_ptr->partition_narrative_mode);
                cJSON_AddNumberToObject(visual, "narrativeBannerTurns",
                    op_ptr->narrative_banner_turns);
            }

            cJSON_AddItemToObject(root, "appOptions", app_options);
        }
    }
    
    // Print to string and write to file
    char* json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        log_error("Failed to print JSON");
        return;
    }
    
    FILE* f = fopen(filename, "w");
    if (!f) {
        log_error("Could not write JSON file: %s", filename);
        cJSON_free(json_string);
        cJSON_Delete(root);
        return;
    }
    
    fprintf(f, "%s\n", json_string);
    fclose(f);
    cJSON_free(json_string);
    cJSON_Delete(root);
    
    log_info("Saved SDL configuration to: %s", filename);
}

void sdl_config_set_default_gamepad_bindings(struct sdl_config* config)
{
    if (!config)
        return;

    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        config->gamepad_button_bindings[i] = GAMEPAD_BIND_NONE;
    }
    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        config->gamepad_trigger_bindings[i] = GAMEPAD_BIND_NONE;
    }
    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        config->gamepad_left_stick_bindings[i] = GAMEPAD_BIND_NONE;
        config->gamepad_right_stick_bindings[i] = GAMEPAD_BIND_NONE;
    }
    sdl_config_clear_gamepad_combo_bindings(config);

    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_SOUTH] = ' ';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_EAST] = 'f';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_WEST] = 'u';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_NORTH] = 's';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = 'e';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'i';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_START] = ESCAPE;
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_BACK] = 'h';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_PADDLE1] = 'r';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_PADDLE2] = 'o';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1] = 'q';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2] = '?';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_LEFT_STICK] = 'z';
    config->gamepad_button_bindings[SDL_GAMEPAD_BUTTON_RIGHT_STICK] = 'j';

    config->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_RIGHT] = 'x';
    config->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_LEFT] = 'a';
    config->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_UP] = 'M';
    config->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_DOWN] = 'b';

    config->gamepad_trigger_bindings[0] = GAMEPAD_BIND_SHIFT;
    config->gamepad_trigger_bindings[1] = GAMEPAD_BIND_CTRL;

    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][SDL_GAMEPAD_BUTTON_SOUTH] = 'Z';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][SDL_GAMEPAD_BUTTON_EAST] = 'F';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][SDL_GAMEPAD_BUTTON_WEST] = 'x';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][SDL_GAMEPAD_BUTTON_NORTH] = 'S';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = 'M';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'p';

    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][SDL_GAMEPAD_BUTTON_SOUTH] = 'z';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][SDL_GAMEPAD_BUTTON_EAST] = '-';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][SDL_GAMEPAD_BUTTON_WEST] = 'X';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][SDL_GAMEPAD_BUTTON_NORTH] = '0';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][SDL_GAMEPAD_BUTTON_BACK] = '\t';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = 'a';
    config->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'j';

    config->gamepad_shoulder_combo_binding = 'l';
}

void sdl_config_set_default_touch_pane_bindings(struct sdl_config* config)
{
    static const int main_defaults[SDL_TOUCH_PANE_BUTTON_COUNT] = {
        ESCAPE, 'S', GAMEPAD_BIND_SHIFT,
        'h', 'i', 'j',
        'u', 's', 'f',
        '7', '8', '9',
        '4', INPUT_BIND_CONFIRM, '6',
        '1', '2', '3',
        'l', 'x', 'a',
        'M', 'h', 'y',
    };
    static const int second_defaults[SDL_TOUCH_PANE_BUTTON_COUNT] = {
        GAMEPAD_BIND_CTRL, 'X', GAMEPAD_BIND_SHIFT,
        '\t', 'e', '-',
        'r', '0', 'F',
        TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT, 'z', TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT,
        'M', 'q', 'p',
        'w', 'b', 'c',
    };
    static const int swipe_defaults[TOUCH_SWIPE_DIR_COUNT] = {
        TOUCH_BIND_TOP_PANEL_CLOSE, TOUCH_BIND_TOP_PANEL_OPEN, '4', '6',
    };
    static const int center_defaults[SDL_TOUCH_ZONE_CENTER_BINDING_COUNT] = {
        'z', 'Z', INPUT_BIND_CONFIRM, 'u',
    };
    static const int corner_action_defaults[SDL_TOUCH_CORNER_ACTION_BINDING_COUNT] = {
        'f', 'F', 'S', 's',
    };
    if (!config)
        return;

    config->touch_profile = SDL_TOUCH_PROFILE_TOUCH_PANE;
    config->touch_pane_default_open = true;
    config->touch_pane_key_labels_visible = false;
    config->touch_pane_inventory_equipment_cycle = true;
    memcpy(config->touch_pane_bindings, main_defaults, sizeof(main_defaults));
    memcpy(config->touch_pane_second_bindings, second_defaults, sizeof(second_defaults));
    SDL_strlcpy(config->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_MAIN], "Main",
        sizeof(config->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_MAIN]));
    SDL_strlcpy(config->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND], "2nd Panel",
        sizeof(config->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND]));
    for (int i = 0; i < SDL_TOUCH_MENU_CATEGORY_COUNT; i++)
        config->touch_menu_command_enabled[i] = true;
    config->touch_movement_mode = SDL_TOUCH_MOVEMENT_ON;
    config->touch_round_movement_enabled = false;
    config->touch_zone_overlay_mode = SDL_TOUCH_ZONE_OVERLAY_MARKERS;
    memcpy(config->touch_zone_center_bindings, center_defaults,
        sizeof(center_defaults));
    config->touch_corner_up_down_side = SDL_TOUCH_CORNER_UP_DOWN_RIGHT;
    memcpy(config->touch_corner_action_bindings, corner_action_defaults,
        sizeof(corner_action_defaults));
    config->touch_top_panel_mode = SDL_TOUCH_TOP_PANEL_MODE_LONG;
    config->touch_top_panel_default_open = false;
    config->touch_top_panel_button_count =
        SDL_TOUCH_TOP_PANEL_BUTTON_COUNT_DEFAULT;
    config->touch_top_panel_tile_scale = SDL_TOUCH_TOP_PANEL_TILE_SCALE_DEFAULT;
    sdl_config_set_default_top_panel_bindings(config);
    config->touch_thumb_enabled = true;
    sdl_config_set_default_thumb_bindings(config);
    config->touch_swipe_enabled = true;
    memcpy(config->touch_swipe_bindings, swipe_defaults, sizeof(swipe_defaults));
}

void sdl_config_clear_touch_pane_labels(struct sdl_config* config)
{
    if (!config)
        return;

    memset(config->touch_pane_labels, 0, sizeof(config->touch_pane_labels));
    memset(config->touch_pane_second_labels, 0, sizeof(config->touch_pane_second_labels));
}

void sdl_config_set_defaults(struct sdl_config* config)
{
    config->main_view_scale = SDL_MAIN_VIEW_PREFERRED_MIN_SCALE;
    config->aux_view_font_size = 0;
    config->margin = 4;
    config->fullscreen = true;
    config->tiles = true;
#if defined(__ANDROID__) || defined(SIL_IOS)
    config->use_unsafe_area = false;
#else
    config->use_unsafe_area = false;
#endif
    config->enable_right_panes = false;
    config->enable_bottom_panes = true;
    config->show_pane_borders = true;
#if defined(__ANDROID__) || defined(SIL_IOS)
    config->left_panel_expanded_on_launch = false;
#else
    config->left_panel_expanded_on_launch = true;
#endif
    config->left_panel_compact_mode = SDL_LEFT_PANEL_COMPACT_COLUMN;
#if defined(__ANDROID__) || defined(SIL_IOS)
    config->min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
#else
    config->min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
#endif
    config->log_pane_display_filter = LOG_HISTORY_FILTER_ALL;
    config->dice_roll_lock_ms = SDL_DICE_ROLL_LOCK_DEFAULT_MS;
    config->dice_roll_overlay_ms = SDL_DICE_ROLL_OVERLAY_DEFAULT_MS;
    
    // Default window position and size (will be overridden by actual screen size)
    config->window_x = -1;  // -1 means centered
    config->window_y = -1;  // -1 means centered
    config->window_width = 0;  // 0 means use default calculation
    config->window_height = 0; // 0 means use default calculation
    
    // Default fonts
    SDL_strlcpy(config->story_font, "lib/xtra/font/Cinzel-Medium.ttf", sizeof(config->story_font));
    SDL_strlcpy(config->story_font2, "lib/xtra/font/EBGaramond-Regular.ttf", sizeof(config->story_font2));
    SDL_strlcpy(config->monospace_font, "lib/xtra/font/VictorMono-Medium.ttf", sizeof(config->monospace_font));
    
    // Default monospace font rendering options
    config->mono_bold = false;
    config->mono_italic = false;
    config->mono_underline = false;
    config->mono_strikethrough = false;
    config->mono_hinting = 0;  // TTF_HINTING_NORMAL
    config->mono_kerning = true;
    config->mono_outline = 0;
    
    // Default story font rendering options
    config->story_bold = false;
    config->story_italic = false;
    config->story_underline = false;
    config->story_strikethrough = false;
    config->story_hinting = 0;  // TTF_HINTING_NORMAL
    config->story_kerning = true;
    config->story_outline = 0;

    // Default second story font rendering options
    config->story2_bold = false;
    config->story2_italic = false;
    config->story2_underline = false;
    config->story2_strikethrough = false;
    config->story2_hinting = 0;  // TTF_HINTING_NORMAL
    config->story2_kerning = true;
    config->story2_outline = 0;
    SDL_strlcpy(config->palette_preset, "classic",
        sizeof(config->palette_preset));
    sdl_config_set_default_keymaps(config);
    sdl_config_set_default_movement_bindings(config,
        SDL_MOVEMENT_PRESET_CLASSIC_SIL);

    // Default gamepad settings
    config->gamepad_enabled = true;
    config->gamepad_auto_mode = true;
    config->steamdeck_mode = false;
    config->steamdeck_inv_equip_same_button_cycle = true;
    config->gamepad_use_dpad = true;
    config->gamepad_use_left_stick = true;
    config->gamepad_deadzone = 12000;
    config->gamepad_trigger_threshold = 16000;
    config->mouse_enabled = true;
    config->mouse_movement_mode = SDL_MOUSE_MOVEMENT_ON;
    sdl_config_set_default_gamepad_bindings(config);
    sdl_config_set_default_touch_pane_bindings(config);
    sdl_config_clear_touch_pane_labels(config);
}

bool sdl_config_set_defaults_for_resolution(struct sdl_config* config,
                                            struct pane_config* pane_configs,
                                            int* pane_count,
                                            int max_panes,
                                            int screen_width,
                                            int screen_height)
{
    (void)pane_configs;
    (void)max_panes;

    // Start with base defaults
    sdl_config_set_defaults(config);
    if (pane_count)
        *pane_count = 0;

    log_info("Using dynamic SDL defaults for %dx%d; resolution presets are disabled",
        screen_width, screen_height);

    return false;
}

void sdl_config_apply_cmdline(struct sdl_config* config, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scale") == 0) {
            if (argc > i + 1) {
                const char* scale_str = argv[++i];
                int scale = atoi(scale_str);
                if (scale > 0) {
                    if (scale < SDL_MAIN_VIEW_MIN_SCALE)
                        scale = SDL_MAIN_VIEW_MIN_SCALE;
                    config->main_view_scale = scale;
                    log_info("Command line: main view scale set to %d", scale);
                }
            }
        } else if (strcmp(argv[i], "--ascii") == 0) {
            config->tiles = false;
            log_info("Command line: ASCII mode enabled");
        } else if (strcmp(argv[i], "--windowed") == 0) {
            config->fullscreen = false;
            log_info("Command line: windowed mode enabled");
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            config->fullscreen = true;
            log_info("Command line: fullscreen mode enabled");
        } else if (strcmp(argv[i], "--tiles") == 0) {
            config->tiles = true;
            log_info("Command line: tiles mode enabled");
        } else if (strcmp(argv[i], "--font-size") == 0) {
            if (argc > i + 1) {
                const char* size_str = argv[++i];
                int size = atoi(size_str);
                if (size > 0) {
                    config->aux_view_font_size = size;
                    log_info("Command line: auxiliary view font size set to %d", size);
                }
            }
        } else if (strcmp(argv[i], "--margin") == 0) {
            if (argc > i + 1) {
                const char* margin_str = argv[++i];
                int margin = atoi(margin_str);
                if (margin >= 0) {
                    config->margin = margin;
                    log_info("Command line: margin set to %d", margin);
                }
            }
        }
    }
}


