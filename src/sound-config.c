#include "sound-config.h"

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

static const char* const legacy_music_main_path = "music/main.wav";
static const char* const default_music_main_path = "music/main.ogg";
static const char* const legacy_music_main_full_path = "music/main_full.wav";
static const char* const default_music_main_full_path = "music/main_full.ogg";
static const char* const legacy_music_ambient_path = "music/ambient.wav";
static const char* const default_music_ambient_path = "music/ambient.ogg";
static const char* const legacy_music_death_path = "sound/death.wav";
static const char* const default_music_death_path = "music/death.ogg";

static float sound_config_clamp_unit(float value)
{
    if (!(value >= 0.0f))
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static void sound_config_sanitize(struct sound_config* config)
{
    if (!config)
        return;

    config->volume_master = sound_config_clamp_unit(config->volume_master);
    config->volume_combat = sound_config_clamp_unit(config->volume_combat);
    config->volume_inventory = sound_config_clamp_unit(config->volume_inventory);
    config->volume_walk = sound_config_clamp_unit(config->volume_walk);
    config->volume_doors = sound_config_clamp_unit(config->volume_doors);
    config->volume_monster_hits =
        sound_config_clamp_unit(config->volume_monster_hits);
    config->volume_traps = sound_config_clamp_unit(config->volume_traps);
    config->volume_other = sound_config_clamp_unit(config->volume_other);
    config->music_main_volume =
        sound_config_clamp_unit(config->music_main_volume);
    config->music_ambient_volume =
        sound_config_clamp_unit(config->music_ambient_volume);

    if (config->sample_rate < 8000 || config->sample_rate > 192000) {
        log_warn("Invalid sound sample rate %d; using 22050",
            config->sample_rate);
        config->sample_rate = 22050;
    }

    if (config->channels != 1 && config->channels != 2) {
        log_warn("Invalid sound channel count %d; using stereo",
            config->channels);
        config->channels = 2;
    }
}

void sound_config_set_defaults(struct sound_config* config)
{
    config->enabled = false;
    config->enable_combat = true;
    config->enable_inventory = true;
    config->enable_walk = true;
    config->enable_doors = true;
    config->enable_monster_hits = true;
    config->enable_traps = true;
    config->volume_master = 1.0f;
    config->volume_combat = 1.0f;
    config->volume_inventory = 1.0f;
    config->volume_walk = 1.0f;
    config->volume_doors = 1.0f;
    config->volume_monster_hits = 1.0f;
    config->volume_traps = 1.0f;
    config->volume_other = 1.0f;
    config->music_main_enabled = true;
    config->music_ambient_enabled = true;
    config->music_main_volume = 1.0f;
    config->music_ambient_volume = 1.0f;
    SDL_strlcpy(config->music_main_path, default_music_main_path, sizeof(config->music_main_path));
    SDL_strlcpy(config->music_main_full_path, default_music_main_full_path, sizeof(config->music_main_full_path));
    SDL_strlcpy(config->music_ambient_path, default_music_ambient_path, sizeof(config->music_ambient_path));
    SDL_strlcpy(config->music_death_path, default_music_death_path,
        sizeof(config->music_death_path));
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
    SDL_IOStream* f = sdl_fopen(filename, "rb");
    if (!f) {
        log_info("Sound config file not found: %s (creating with defaults)", filename);
        // Auto-create sound.json with defaults
        sound_config_save(filename, config);
        return;
    }

    Sint64 file_size = SDL_GetIOSize(f);
    if (file_size < 0) {
        log_error("Failed to get sound config size: %s", filename);
        sdl_fclose(f);
        return;
    }

    if (file_size > 16 * 1024 * 1024) {
        log_error("Sound config file too large: %s", filename);
        sdl_fclose(f);
        return;
    }

    size_t length = (size_t)file_size;
    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        sdl_fclose(f);
        log_error("Failed to allocate memory for sound config");
        return;
    }

    size_t read = SDL_ReadIO(f, buffer, length);
    if (read != length) {
        log_warn("Sound config read truncated: %s (expected %zu, got %zu)", filename, length, read);
    }
    buffer[read] = '\0';
    sdl_fclose(f);
    
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

    // Load monster hit sounds flag
    cJSON* enable_monster_hits = cJSON_GetObjectItemCaseSensitive(root, "enableMonsterHits");
    if (cJSON_IsBool(enable_monster_hits)) {
        config->enable_monster_hits = cJSON_IsTrue(enable_monster_hits);
        log_debug("Loaded sound enable_monster_hits: %s", config->enable_monster_hits ? "true" : "false");
    }

    // Load trap sounds flag
    cJSON* enable_traps = cJSON_GetObjectItemCaseSensitive(root, "enableTraps");
    if (cJSON_IsBool(enable_traps)) {
        config->enable_traps = cJSON_IsTrue(enable_traps);
        log_debug("Loaded sound enable_traps: %s", config->enable_traps ? "true" : "false");
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

    cJSON* volume_monster_hits = cJSON_GetObjectItemCaseSensitive(root, "volumeMonsterHits");
    if (cJSON_IsNumber(volume_monster_hits)) {
        config->volume_monster_hits = (float)volume_monster_hits->valuedouble;
        log_debug("Loaded monster hits volume: %.2f", config->volume_monster_hits);
    }

    cJSON* volume_traps = cJSON_GetObjectItemCaseSensitive(root, "volumeTraps");
    if (cJSON_IsNumber(volume_traps)) {
        config->volume_traps = (float)volume_traps->valuedouble;
        log_debug("Loaded traps volume: %.2f", config->volume_traps);
    }

    cJSON* volume_other = cJSON_GetObjectItemCaseSensitive(root, "volumeOther");
    if (cJSON_IsNumber(volume_other)) {
        config->volume_other = (float)volume_other->valuedouble;
        log_debug("Loaded other volume: %.2f", config->volume_other);
    }

    cJSON* music_main_enabled = cJSON_GetObjectItemCaseSensitive(root, "music_main_enabled");
    if (cJSON_IsBool(music_main_enabled)) {
        config->music_main_enabled = cJSON_IsTrue(music_main_enabled);
    }

    cJSON* music_ambient_enabled = cJSON_GetObjectItemCaseSensitive(root, "music_ambient_enabled");
    if (cJSON_IsBool(music_ambient_enabled)) {
        config->music_ambient_enabled = cJSON_IsTrue(music_ambient_enabled);
    }

    cJSON* music_main_volume = cJSON_GetObjectItemCaseSensitive(root, "music_main_volume");
    if (cJSON_IsNumber(music_main_volume)) {
        config->music_main_volume = (float)music_main_volume->valuedouble;
    }

    cJSON* music_ambient_volume = cJSON_GetObjectItemCaseSensitive(root, "music_ambient_volume");
    if (cJSON_IsNumber(music_ambient_volume)) {
        config->music_ambient_volume = (float)music_ambient_volume->valuedouble;
    }

    cJSON* music_main_path = cJSON_GetObjectItemCaseSensitive(root, "music_main_path");
    if (cJSON_IsString(music_main_path) && music_main_path->valuestring) {
        SDL_strlcpy(config->music_main_path, music_main_path->valuestring, sizeof(config->music_main_path));
    }
    if (streq(config->music_main_path, legacy_music_main_path)) {
        SDL_strlcpy(config->music_main_path, default_music_main_path,
            sizeof(config->music_main_path));
    }

    cJSON* music_main_full_path = cJSON_GetObjectItemCaseSensitive(root, "music_main_full_path");
    if (cJSON_IsString(music_main_full_path) && music_main_full_path->valuestring) {
        SDL_strlcpy(config->music_main_full_path, music_main_full_path->valuestring, sizeof(config->music_main_full_path));
    }
    if (streq(config->music_main_full_path, legacy_music_main_full_path)) {
        SDL_strlcpy(config->music_main_full_path, default_music_main_full_path,
            sizeof(config->music_main_full_path));
    }

    cJSON* music_ambient_path = cJSON_GetObjectItemCaseSensitive(root, "music_ambient_path");
    if (cJSON_IsString(music_ambient_path) && music_ambient_path->valuestring) {
        SDL_strlcpy(config->music_ambient_path, music_ambient_path->valuestring, sizeof(config->music_ambient_path));
    }
    if (streq(config->music_ambient_path, legacy_music_ambient_path)) {
        SDL_strlcpy(config->music_ambient_path, default_music_ambient_path,
            sizeof(config->music_ambient_path));
    }

    cJSON* music_death_path = cJSON_GetObjectItemCaseSensitive(root, "music_death_path");
    if (cJSON_IsString(music_death_path) && music_death_path->valuestring) {
        SDL_strlcpy(config->music_death_path, music_death_path->valuestring, sizeof(config->music_death_path));
    }
    if (streq(config->music_death_path, legacy_music_death_path)) {
        SDL_strlcpy(config->music_death_path, default_music_death_path,
            sizeof(config->music_death_path));
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

    sound_config_sanitize(config);
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
    cJSON_AddBoolToObject(root, "enableMonsterHits", config->enable_monster_hits);
    cJSON_AddBoolToObject(root, "enableTraps", config->enable_traps);
    cJSON_AddNumberToObject(root, "volumeMaster", config->volume_master);
    cJSON_AddNumberToObject(root, "volumeCombat", config->volume_combat);
    cJSON_AddNumberToObject(root, "volumeInventory", config->volume_inventory);
    cJSON_AddNumberToObject(root, "volumeWalk", config->volume_walk);
    cJSON_AddNumberToObject(root, "volumeDoors", config->volume_doors);
    cJSON_AddNumberToObject(root, "volumeMonsterHits", config->volume_monster_hits);
    cJSON_AddNumberToObject(root, "volumeTraps", config->volume_traps);
    cJSON_AddNumberToObject(root, "volumeOther", config->volume_other);
    cJSON_AddBoolToObject(root, "music_main_enabled", config->music_main_enabled);
    cJSON_AddBoolToObject(root, "music_ambient_enabled", config->music_ambient_enabled);
    cJSON_AddNumberToObject(root, "music_main_volume", config->music_main_volume);
    cJSON_AddNumberToObject(root, "music_ambient_volume", config->music_ambient_volume);
    cJSON_AddStringToObject(root, "music_main_path", config->music_main_path);
    cJSON_AddStringToObject(root, "music_main_full_path", config->music_main_full_path);
    cJSON_AddStringToObject(root, "music_ambient_path", config->music_ambient_path);
    cJSON_AddStringToObject(root, "music_death_path", config->music_death_path);
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
    
    SDL_IOStream* f = sdl_fopen(filename, "wb");
    if (!f) {
        log_error("Could not write sound config JSON file: %s", filename);
        cJSON_free(json_string);
        cJSON_Delete(root);
        return;
    }

    size_t json_len = strlen(json_string);
    size_t written = SDL_WriteIO(f, json_string, json_len);
    if (written != json_len) {
        log_error("Failed writing sound config JSON file: %s", filename);
        sdl_fclose(f);
        cJSON_free(json_string);
        cJSON_Delete(root);
        return;
    }
    sdl_fclose(f);
    
    cJSON_free(json_string);
    cJSON_Delete(root);
    
    log_info("Sound configuration saved to %s", filename);
}
