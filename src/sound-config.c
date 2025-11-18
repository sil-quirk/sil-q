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
    config->sample_rate = 22050;
    config->channels = 2;
    SDL_strlcpy(config->format, "s16", sizeof(config->format));
    
    // Clear all sound event folders
    for (int i = 0; i < 37; i++) {
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
        log_info("Sound config file not found: %s (using defaults)", filename);
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
            for (int i = 0; i < 37; i++) {
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
    cJSON_AddNumberToObject(root, "sampleRate", config->sample_rate);
    cJSON_AddNumberToObject(root, "channels", config->channels);
    cJSON_AddStringToObject(root, "format", config->format);
    
    // Save sound events
    cJSON* events = cJSON_CreateObject();
    if (events) {
        extern const cptr angband_sound_name[];
        for (int i = 0; i < 37; i++) {
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
