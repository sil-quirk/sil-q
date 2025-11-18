#pragma once

#include <stdbool.h>

// Sound configuration structure
struct sound_config {
    bool enabled;              // Enable/disable game sounds (default: false)
    int sample_rate;           // Audio sample rate (default: 22050)
    int channels;              // Audio channels: 1=mono, 2=stereo (default: 2)
    char format[16];           // Audio format: "s8", "u8", "s16", "s32", "f32" (default: "s16")
    char events[54][256];      // Folder paths for each sound event (MSG_MAX entries)
};

// Load sound configuration from JSON file
void sound_config_load(const char* filename, struct sound_config* config);

// Save sound configuration to JSON file
void sound_config_save(const char* filename, const struct sound_config* config);

// Set default sound configuration values
void sound_config_set_defaults(struct sound_config* config);
