#pragma once

#include <stdbool.h>

#define SOUND_CONFIG_EVENT_MAX 66

// Sound configuration structure
struct sound_config {
    bool enabled;              // Enable/disable game sounds (default: false)
    bool enable_combat;        // Enable combat sounds
    bool enable_inventory;     // Enable inventory sounds
    bool enable_walk;          // Enable walk sounds
    bool enable_doors;         // Enable door sounds
    float volume_master;       // Master volume (0.0-1.0, default: 1.0)
    float volume_combat;       // Combat sounds volume (0.0-1.0, default: 1.0)
    float volume_inventory;    // Inventory sounds volume (0.0-1.0, default: 1.0)
    float volume_walk;         // Walk sounds volume (0.0-1.0, default: 1.0)
    float volume_doors;        // Door sounds volume (0.0-1.0, default: 1.0)
    float volume_other;        // Other sounds volume (0.0-1.0, default: 1.0)
    bool music_main_enabled;
    bool music_ambient_enabled;
    float music_main_volume;
    float music_ambient_volume;
    char music_main_path[256];
    char music_main_full_path[256];
    char music_ambient_path[256];
    char music_death_path[256];
    int sample_rate;           // Audio sample rate (default: 22050)
    int channels;              // Audio channels: 1=mono, 2=stereo (default: 2)
    char format[16];           // Audio format: "s8", "u8", "s16", "s32", "f32" (default: "s16")
    char events[SOUND_CONFIG_EVENT_MAX][256]; // Folder paths for each sound event (MSG_MAX entries)
};

// Load sound configuration from JSON file
void sound_config_load(const char* filename, struct sound_config* config);

// Save sound configuration to JSON file
void sound_config_save(const char* filename, const struct sound_config* config);

// Set default sound configuration values
void sound_config_set_defaults(struct sound_config* config);
