#pragma once

#include <stdbool.h>

/* Initialize the SDL sound subsystem and load the sound registry. */
bool sdl_sound_initialize(void);

/* Reload sound configuration data (safe to call multiple times). */
void sdl_sound_reload(void);

/* Release any audio resources owned by the SDL sound subsystem. */
void sdl_sound_shutdown(void);

/* Play the sound mapped to the specified Angband message index. */
void sdl_sound_handle(int sound_idx);
