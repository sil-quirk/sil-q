#pragma once

#include <stdbool.h>
#include "sound-config.h"

/* Initialize the SDL sound subsystem and load the sound registry. */
bool sdl_sound_initialize(void);

/* Reload sound configuration data (safe to call multiple times). */
void sdl_sound_reload(void);

/* Release any audio resources owned by the SDL sound subsystem. */
void sdl_sound_shutdown(void);

/* Play the sound mapped to the specified Angband message index. */
void sdl_sound_handle(int sound_idx);

/* Get pointer to global sound configuration (for UI) */
struct sound_config* sdl_sound_get_config(void);

/* Save sound configuration to disk and update internal state */
void sdl_sound_save_config(void);

/* Music control functions */
void sdl_music_play_main(void);
void sdl_music_play_main_full(void);
void sdl_music_play_menu_theme(void);
void sdl_music_play_ambient(void);
void sdl_music_stop_main(void);
void sdl_music_stop_ambient(void);
void sdl_music_update(void); /* Call periodically to handle music looping */
void sdl_music_update_volumes(void); /* Update volume of currently playing music */
