#include "sound-config.h"

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

void sound_config_set_defaults(struct sound_config* config)
{
    config->enabled = false;
    config->enable_combat = true;
    config->enable_inventory = true;
    config->enable_walk = true;
    config->enable_doors = true;
    config->volume_master = 1.0f;
    config->volume_combat = 1.0f;
    config->volume_inventory = 1.0f;
    config->volume_walk = 1.0f;
    config->volume_doors = 1.0f;
    config->volume_other = 1.0f;
    config->sample_rate = 22050;
    config->channels = 2;
    SDL_strlcpy(config->format, "s16", sizeof(config->format));
    
    // Clear all sound event folders
    for (int i = 0; i < MSG_MAX; i++) {
        config->events[i][0] = '\0';
    }
}

void sound_config_load(const char* filename, struct sound_config* config)
{
    // Start with defaults
    sound_config_set_defaults(config);
    
    if (!filename || !filename[0]) {
        log_warn("No sound config filename provided");
        return;
    }
    
    // Read file
    FILE* f = fopen(filename, "r");
    if (!f) {
        log_info("Sound config file not found: %s (creating with defaults)", filename);
        // Auto-create sound.json with defaults
        sound_config_save(filename, config);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fclose(f);
        log_error("Failed to allocate memory for sound config");
        return;
    }
    
    size_t read = fread(buffer, 1, length, f);
    buffer[read] = '\0';
    fclose(f);
    
    // Parse JSON
    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!root) {
        log_error("Failed to parse sound config JSON: %s", filename);
        return;
    }
    
    // Load enabled flag
    cJSON* enabled = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(enabled)) {
        config->enabled = cJSON_IsTrue(enabled);
        log_debug("Loaded sound enabled: %s", config->enabled ? "true" : "false");
    }

    // Load combat sounds flag
    cJSON* enable_combat = cJSON_GetObjectItemCaseSensitive(root, "enableCombat");
    if (cJSON_IsBool(enable_combat)) {
        config->enable_combat = cJSON_IsTrue(enable_combat);
        log_debug("Loaded sound enable_combat: %s", config->enable_combat ? "true" : "false");
    }

    // Load inventory sounds flag
    cJSON* enable_inventory = cJSON_GetObjectItemCaseSensitive(root, "enableInventory");
    if (cJSON_IsBool(enable_inventory)) {
        config->enable_inventory = cJSON_IsTrue(enable_inventory);
        log_debug("Loaded sound enable_inventory: %s", config->enable_inventory ? "true" : "false");
    }

    // Load walk sounds flag
    cJSON* enable_walk = cJSON_GetObjectItemCaseSensitive(root, "enableWalk");
    if (cJSON_IsBool(enable_walk)) {
        config->enable_walk = cJSON_IsTrue(enable_walk);
        log_debug("Loaded sound enable_walk: %s", config->enable_walk ? "true" : "false");
    }

    // Load door sounds flag
    cJSON* enable_doors = cJSON_GetObjectItemCaseSensitive(root, "enableDoors");
    if (cJSON_IsBool(enable_doors)) {
        config->enable_doors = cJSON_IsTrue(enable_doors);
        log_debug("Loaded sound enable_doors: %s", config->enable_doors ? "true" : "false");
    }

    // Load volume settings
    cJSON* volume_master = cJSON_GetObjectItemCaseSensitive(root, "volumeMaster");
    if (cJSON_IsNumber(volume_master)) {
        config->volume_master = (float)volume_master->valuedouble;
        log_debug("Loaded master volume: %.2f", config->volume_master);
    }

    cJSON* volume_combat = cJSON_GetObjectItemCaseSensitive(root, "volumeCombat");
    if (cJSON_IsNumber(volume_combat)) {
        config->volume_combat = (float)volume_combat->valuedouble;
        log_debug("Loaded combat volume: %.2f", config->volume_combat);
    }

    cJSON* volume_inventory = cJSON_GetObjectItemCaseSensitive(root, "volumeInventory");
    if (cJSON_IsNumber(volume_inventory)) {
        config->volume_inventory = (float)volume_inventory->valuedouble;
        log_debug("Loaded inventory volume: %.2f", config->volume_inventory);
    }

    cJSON* volume_walk = cJSON_GetObjectItemCaseSensitive(root, "volumeWalk");
    if (cJSON_IsNumber(volume_walk)) {
        config->volume_walk = (float)volume_walk->valuedouble;
        log_debug("Loaded walk volume: %.2f", config->volume_walk);
    }

    cJSON* volume_doors = cJSON_GetObjectItemCaseSensitive(root, "volumeDoors");
    if (cJSON_IsNumber(volume_doors)) {
        config->volume_doors = (float)volume_doors->valuedouble;
        log_debug("Loaded doors volume: %.2f", config->volume_doors);
    }

    cJSON* volume_other = cJSON_GetObjectItemCaseSensitive(root, "volumeOther");
    if (cJSON_IsNumber(volume_other)) {
        config->volume_other = (float)volume_other->valuedouble;
        log_debug("Loaded other volume: %.2f", config->volume_other);
    }
    
    // Load sample rate
    cJSON* sample_rate = cJSON_GetObjectItemCaseSensitive(root, "sampleRate");
    if (cJSON_IsNumber(sample_rate)) {
        config->sample_rate = sample_rate->valueint;
        log_debug("Loaded sound sample rate: %d", config->sample_rate);
    }
    
    // Load channels
    cJSON* channels = cJSON_GetObjectItemCaseSensitive(root, "channels");
    if (cJSON_IsNumber(channels)) {
        config->channels = channels->valueint;
        log_debug("Loaded sound channels: %d", config->channels);
    }
    
    // Load format
    cJSON* format = cJSON_GetObjectItemCaseSensitive(root, "format");
    if (cJSON_IsString(format) && format->valuestring) {
        SDL_strlcpy(config->format, format->valuestring, sizeof(config->format));
        log_debug("Loaded sound format: %s", config->format);
    }
    
    // Load sound events object
    cJSON* sound_events = cJSON_GetObjectItemCaseSensitive(root, "events");
    if (cJSON_IsObject(sound_events)) {
        extern const cptr angband_sound_name[];
        cJSON* event = NULL;
        cJSON_ArrayForEach(event, sound_events) {
            const char* event_name = event->string;
            if (!event_name || !cJSON_IsString(event)) continue;
            
            // Find the event index
            for (int i = 0; i < MSG_MAX; i++) {
                if (angband_sound_name[i] && streq(angband_sound_name[i], event_name)) {
                    SDL_strlcpy(config->events[i], event->valuestring, sizeof(config->events[i]));
                    log_debug("Loaded sound event '%s' -> folder '%s'", event_name, event->valuestring);
                    break;
                }
            }
        }
    }
    
    cJSON_Delete(root);
    log_info("Sound configuration loaded from %s", filename);
}

void sound_config_save(const char* filename, const struct sound_config* config)
{
    if (!filename || !filename[0]) {
        log_error("No sound config filename provided for saving");
        return;
    }
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        log_error("Failed to create sound config JSON root object");
        return;
    }
    
    // Save basic settings
    cJSON_AddBoolToObject(root, "enabled", config->enabled);
    cJSON_AddBoolToObject(root, "enableCombat", config->enable_combat);
    cJSON_AddBoolToObject(root, "enableInventory", config->enable_inventory);
    cJSON_AddBoolToObject(root, "enableWalk", config->enable_walk);
    cJSON_AddBoolToObject(root, "enableDoors", config->enable_doors);
    cJSON_AddNumberToObject(root, "volumeMaster", config->volume_master);
    cJSON_AddNumberToObject(root, "volumeCombat", config->volume_combat);
    cJSON_AddNumberToObject(root, "volumeInventory", config->volume_inventory);
    cJSON_AddNumberToObject(root, "volumeWalk", config->volume_walk);
    cJSON_AddNumberToObject(root, "volumeDoors", config->volume_doors);
    cJSON_AddNumberToObject(root, "volumeOther", config->volume_other);
    cJSON_AddNumberToObject(root, "sampleRate", config->sample_rate);
    cJSON_AddNumberToObject(root, "channels", config->channels);
    cJSON_AddStringToObject(root, "format", config->format);
    
    // Save sound events
    cJSON* events = cJSON_CreateObject();
    if (events) {
        extern const cptr angband_sound_name[];
        for (int i = 0; i < MSG_MAX; i++) {
            if (config->events[i][0] && angband_sound_name[i] && angband_sound_name[i][0]) {
                cJSON_AddStringToObject(events, angband_sound_name[i], config->events[i]);
            }
        }
        cJSON_AddItemToObject(root, "events", events);
    }
    
    // Print to string and write to file
    char* json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        log_error("Failed to print sound config JSON");
        return;
    }
    
    FILE* f = fopen(filename, "w");
    if (!f) {
        log_error("Could not write sound config JSON file: %s", filename);
        cJSON_free(json_string);
        cJSON_Delete(root);
        return;
    }
    
    fputs(json_string, f);
    fclose(f);
    
    cJSON_free(json_string);
    cJSON_Delete(root);
    
    log_info("Sound configuration saved to %s", filename);
}
