#include "sdl-sound.h"

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "sound-config.h"
#include <SDL3/SDL.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define SDL_SOUND_MAX_VARIANTS 64
#define SDL_SOUND_NAME_LEN 256
#define SDL_SOUND_MAX_ACTIVE_STREAMS 16

typedef struct {
    char sound_files[MSG_MAX][SDL_SOUND_MAX_VARIANTS][SDL_SOUND_NAME_LEN];
    int sound_counts[MSG_MAX];
} sound_bank;

static struct {
    SDL_AudioDeviceID device;
    SDL_AudioSpec device_spec;
    sound_bank bank;
    bool bank_loaded;
    SDL_AudioStream* active_streams[SDL_SOUND_MAX_ACTIVE_STREAMS];
    bool enable_combat;
    bool enable_inventory;
    bool enable_walk;
    bool enable_doors;
    float volume_combat;
    float volume_inventory;
    float volume_walk;
    float volume_doors;
    float volume_other;
    SDL_AudioStream* music_main_stream;
    SDL_AudioStream* music_ambient_stream;
    char music_main_path[1024];
    char music_ambient_path[1024];
    float music_main_volume;
    float music_ambient_volume;
    bool music_main_enabled;
    bool music_ambient_enabled;
    Uint8* music_main_buffer;
    Uint32 music_main_length;
    SDL_AudioSpec music_main_spec;
    Uint8* music_ambient_buffer;
    Uint32 music_ambient_length;
    SDL_AudioSpec music_ambient_spec;
} sound_state;

static struct sound_config g_sound_config; /* Global config for UI access */
static char g_sound_config_path[1024]; /* Path to sound.json */

static void sdl_sound_reset_bank(void);
static bool sdl_sound_load_from_config(const struct sound_config* config);
static bool sdl_sound_scan_folder(const char* folder_path, char files[][SDL_SOUND_NAME_LEN], int* file_count, int max_files);
static bool sdl_sound_is_audio_file(const char* filename);
static void sdl_sound_build_path(const char* base_path, char* dst, size_t dst_len);
static void sdl_sound_clear_streams(void);
static bool sdl_sound_track_stream(SDL_AudioStream* stream);
static void sdl_sound_destroy_stream(SDL_AudioStream** stream);
static void sdl_music_stop(SDL_AudioStream** stream_ptr);

static bool is_sound_enabled(int sound_idx)
{
    // Combat
    if (sound_idx == MSG_HIT || sound_idx == MSG_SHOOT || sound_idx == MSG_DIG ||
        (sound_idx >= MSG_WEAPON_SLASH_LIGHT && sound_idx <= MSG_WEAPON_UNARMED) ||
        sound_idx == MSG_WEAPON_SLASH_MEDIUM || sound_idx == MSG_MISS || sound_idx == MSG_KILL) {
        return sound_state.enable_combat;
    }
    
    // Inventory
    if (sound_idx == MSG_DROP || sound_idx == MSG_QUAFF || sound_idx == MSG_ZAP ||
        sound_idx == MSG_EAT || sound_idx == MSG_PICK || sound_idx == MSG_ARMOR ||
        (sound_idx >= MSG_EQUIP_SWORD && sound_idx <= MSG_UNEQUIP_JEWELRY) ||
        (sound_idx >= MSG_DROP_GLASS && sound_idx <= MSG_ACTIVATE)) {
        return sound_state.enable_inventory;
    }
    
    // Walk
    if (sound_idx == MSG_WALK) {
        return sound_state.enable_walk;
    }
    
    // Doors
    if (sound_idx == MSG_OPENDOOR || sound_idx == MSG_SHUTDOOR || sound_idx == MSG_BASHDOOR ||
        sound_idx == MSG_HITWALL || sound_idx == MSG_NOTHING_TO_OPEN || sound_idx == MSG_LOCKPICK_FAIL) {
        return sound_state.enable_doors;
    }
    
    return true; // Default enabled for other sounds
}

static float get_sound_volume(int sound_idx)
{
    // Combat
    if (sound_idx == MSG_HIT || sound_idx == MSG_SHOOT || sound_idx == MSG_DIG ||
        (sound_idx >= MSG_WEAPON_SLASH_LIGHT && sound_idx <= MSG_WEAPON_UNARMED) ||
        sound_idx == MSG_WEAPON_SLASH_MEDIUM || sound_idx == MSG_MISS || sound_idx == MSG_KILL) {
        return sound_state.volume_combat;
    }
    
    // Inventory
    if (sound_idx == MSG_DROP || sound_idx == MSG_QUAFF || sound_idx == MSG_ZAP ||
        sound_idx == MSG_EAT || sound_idx == MSG_PICK || sound_idx == MSG_ARMOR ||
        (sound_idx >= MSG_EQUIP_SWORD && sound_idx <= MSG_UNEQUIP_JEWELRY) ||
        (sound_idx >= MSG_DROP_GLASS && sound_idx <= MSG_ACTIVATE)) {
        return sound_state.volume_inventory;
    }
    
    // Walk
    if (sound_idx == MSG_WALK) {
        return sound_state.volume_walk;
    }
    
    // Doors
    if (sound_idx == MSG_OPENDOOR || sound_idx == MSG_SHUTDOOR || sound_idx == MSG_BASHDOOR ||
        sound_idx == MSG_HITWALL || sound_idx == MSG_NOTHING_TO_OPEN || sound_idx == MSG_LOCKPICK_FAIL) {
        return sound_state.volume_doors;
    }
    
    return sound_state.volume_other; // Default volume for other sounds (includes MSG_LEVEL)
}

static void sdl_sound_reset_bank(void)
{
    for (int i = 0; i < MSG_MAX; i++) {
        sound_state.bank.sound_counts[i] = 0;
        for (int j = 0; j < SDL_SOUND_MAX_VARIANTS; j++) {
            sound_state.bank.sound_files[i][j][0] = '\0';
        }
    }
}

static bool sdl_sound_is_audio_file(const char* filename)
{
    if (!filename || !filename[0]) return false;
    
    size_t len = strlen(filename);
    if (len < 5) return false;
    
    const char* ext = filename + len - 4;
    /* Only WAV files are playable via SDL_LoadWAV; omit OGG here to avoid
     * false-positive matches that would later fail at load time.
     */
    return (SDL_strcasecmp(ext, ".wav") == 0);
}

static void sdl_sound_build_path(const char* base_path, char* dst, size_t dst_len)
{
    if (!base_path || !base_path[0]) {
        dst[0] = '\0';
        return;
    }
    
    // Check if it's an absolute path
    bool is_absolute = false;
#ifdef _WIN32
    if (((base_path[0] >= 'A' && base_path[0] <= 'Z') || 
         (base_path[0] >= 'a' && base_path[0] <= 'z')) && 
        base_path[1] == ':') {
        is_absolute = true;
    }
#endif
    if (base_path[0] == '/' || base_path[0] == '\\') {
        is_absolute = true;
    }
    
    if (is_absolute) {
        SDL_strlcpy(dst, base_path, dst_len);
        log_debug("Music path (absolute): '%s'", dst);
    } else {
        const char* anchor = ANGBAND_DIR_XTRA;
        if (!anchor || !anchor[0]) {
            dst[0] = '\0';
            return;
        }
        path_build(dst, dst_len, anchor, base_path);
        log_debug("Music path built: anchor='%s' + base='%s' => '%s'", anchor, base_path, dst);
    }
}

static bool sdl_sound_scan_folder(const char* folder_path, char files[][SDL_SOUND_NAME_LEN], int* file_count, int max_files)
{
    *file_count = 0;
    
    if (!folder_path || !folder_path[0]) {
        return false;
    }
    
#ifdef _WIN32
    char search_path[1024];
    strnfmt(search_path, sizeof(search_path), "%s\\*", folder_path);
    
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    
    if (h_find == INVALID_HANDLE_VALUE) {
        log_debug("Cannot open sound folder: %s", folder_path);
        return false;
    }
    
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (sdl_sound_is_audio_file(find_data.cFileName) && *file_count < max_files) {
                strnfmt(files[*file_count], SDL_SOUND_NAME_LEN, "%s\\%s", folder_path, find_data.cFileName);
                (*file_count)++;
            }
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    
    FindClose(h_find);
#else
    DIR* dir = opendir(folder_path);
    if (!dir) {
        log_debug("Cannot open sound folder: %s", folder_path);
        return false;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && *file_count < max_files) {
        if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
            if (sdl_sound_is_audio_file(entry->d_name)) {
                strnfmt(files[*file_count], SDL_SOUND_NAME_LEN, "%s/%s", folder_path, entry->d_name);
                (*file_count)++;
            }
        }
    }
    
    closedir(dir);
#endif
    
    log_debug("Scanned folder '%s': found %d audio file(s)", folder_path, *file_count);
    return *file_count > 0;
}

static bool sdl_sound_load_from_config(const struct sound_config* config)
{
    if (!config) {
        return false;
    }
    
    sdl_sound_reset_bank();
    
    extern const cptr angband_sound_name[];
    int loaded_events = 0;
    
    for (int i = 0; i < MSG_MAX; i++) {
        if (!config->events[i][0]) {
            continue; // No folder configured for this event
        }
        
        char folder_path[1024];
        sdl_sound_build_path(config->events[i], folder_path, sizeof(folder_path));
        
        if (!folder_path[0]) {
            log_debug("Invalid path for sound event %d ('%s')", i, 
                      angband_sound_name[i] ? angband_sound_name[i] : "unknown");
            continue;
        }
        
        int count = 0;
        if (sdl_sound_scan_folder(folder_path, sound_state.bank.sound_files[i], &count, SDL_SOUND_MAX_VARIANTS)) {
            sound_state.bank.sound_counts[i] = count;
            loaded_events++;
            log_debug("Sound event '%s' (idx=%d): loaded %d file(s) from '%s'", 
                      angband_sound_name[i] ? angband_sound_name[i] : "unknown",
                      i, count, folder_path);
        }
    }
    
    log_info("Loaded sound events: %d/%d", loaded_events, MSG_MAX);
    return loaded_events > 0;
}

static void sdl_sound_destroy_stream(SDL_AudioStream** stream_ptr)
{
    if (!stream_ptr || !*stream_ptr) {
        return;
    }
    SDL_AudioStream* stream = *stream_ptr;
    SDL_UnbindAudioStream(stream);
    SDL_DestroyAudioStream(stream);
    *stream_ptr = NULL;
}

static void sdl_sound_clear_streams(void)
{
    for (int i = 0; i < SDL_SOUND_MAX_ACTIVE_STREAMS; i++) {
        sdl_sound_destroy_stream(&sound_state.active_streams[i]);
    }
}

static bool sdl_sound_track_stream(SDL_AudioStream* stream)
{
    if (!stream) {
        return false;
    }
    
    /* First, try to find a free slot without destroying anything */
    for (int i = 0; i < SDL_SOUND_MAX_ACTIVE_STREAMS; i++) {
        if (!sound_state.active_streams[i]) {
            sound_state.active_streams[i] = stream;
            return true;
        }
    }

    /* No free slots - prune finished streams and try again */
    for (int i = 0; i < SDL_SOUND_MAX_ACTIVE_STREAMS; i++) {
        SDL_AudioStream* existing = sound_state.active_streams[i];
        if (!existing) continue;
        
        int available = SDL_GetAudioStreamAvailable(existing);
        int queued = SDL_GetAudioStreamQueued(existing);
        
        /* Only destroy if truly empty - both input and output are drained */
        if (available <= 0 && queued <= 0) {
            sdl_sound_destroy_stream(&sound_state.active_streams[i]);
            sound_state.active_streams[i] = stream;
            return true;
        }
    }

    /* Still no space: drop the oldest (slot 0) as last resort */
    sdl_sound_destroy_stream(&sound_state.active_streams[0]);
    sound_state.active_streams[0] = stream;
    log_warn("Sound mixer saturated; dropping oldest playing sound");
    return true;
}

static SDL_AudioFormat sdl_sound_parse_format(const char* format_str)
{
    if (!format_str || !format_str[0]) {
        return SDL_AUDIO_S16;
    }
    
    char lowered[16];
    SDL_strlcpy(lowered, format_str, sizeof(lowered));
    for (char* p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);
    
    if (streq(lowered, "s8")) return SDL_AUDIO_S8;
    if (streq(lowered, "u8")) return SDL_AUDIO_U8;
    if (streq(lowered, "s16")) return SDL_AUDIO_S16;
    if (streq(lowered, "s32")) return SDL_AUDIO_S32;
    if (streq(lowered, "f32")) return SDL_AUDIO_F32;
    
    return SDL_AUDIO_S16; // Default
}

bool sdl_sound_initialize(void)
{
    return true; // Defer actual initialization until sdl_sound_reload()
}

void sdl_sound_reload(void)
{
    sdl_sound_clear_streams();
    sdl_sound_reset_bank();
    
    // Build path to sound.json
#ifdef SIL_USE_LOCAL_DATA
    if (ANGBAND_DIR_PREF && ANGBAND_DIR_PREF[0]) {
        path_build(g_sound_config_path, sizeof(g_sound_config_path), ANGBAND_DIR_PREF, "sound.json");
    } else {
        SDL_strlcpy(g_sound_config_path, "sound.json", sizeof(g_sound_config_path));
    }
#else
    if (ANGBAND_DIR_USER && ANGBAND_DIR_USER[0]) {
        path_build(g_sound_config_path, sizeof(g_sound_config_path), ANGBAND_DIR_USER, "sound.json");
    } else {
        SDL_strlcpy(g_sound_config_path, "sound.json", sizeof(g_sound_config_path));
    }
#endif
    
    // Load sound configuration from sound.json
    sound_config_load(g_sound_config_path, &g_sound_config);
    sound_state.bank_loaded = sdl_sound_load_from_config(&g_sound_config);
    
    // Copy group flags and volumes to sound_state
    sound_state.enable_combat = g_sound_config.enable_combat;
    sound_state.enable_inventory = g_sound_config.enable_inventory;
    sound_state.enable_walk = g_sound_config.enable_walk;
    sound_state.enable_doors = g_sound_config.enable_doors;
    sound_state.volume_combat = g_sound_config.volume_combat;
    sound_state.volume_inventory = g_sound_config.volume_inventory;
    sound_state.volume_walk = g_sound_config.volume_walk;
    sound_state.volume_doors = g_sound_config.volume_doors;
    sound_state.volume_other = g_sound_config.volume_other;
    sound_state.music_main_enabled = g_sound_config.music_main_enabled;
    sound_state.music_ambient_enabled = g_sound_config.music_ambient_enabled;
    sound_state.music_main_volume = g_sound_config.music_main_volume;
    sound_state.music_ambient_volume = g_sound_config.music_ambient_volume;
    sdl_sound_build_path(g_sound_config.music_main_path, sound_state.music_main_path, sizeof(sound_state.music_main_path));
    sdl_sound_build_path(g_sound_config.music_ambient_path, sound_state.music_ambient_path, sizeof(sound_state.music_ambient_path));

    log_debug("Sound config: enabled=%d, device=%u", g_sound_config.enabled, sound_state.device);
    log_debug("Music paths: main='%s', ambient='%s'", sound_state.music_main_path, sound_state.music_ambient_path);

    // Open audio device if not already open and sound is enabled
    if (g_sound_config.enabled && !sound_state.device) {
        log_debug("Opening audio device...");
        SDL_AudioSpec desired;
        SDL_zero(desired);
        desired.freq = g_sound_config.sample_rate;
        desired.format = sdl_sound_parse_format(g_sound_config.format);
        desired.channels = g_sound_config.channels;

        sound_state.device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (!sound_state.device) {
            log_warn("Failed to open audio device: %s", SDL_GetError());
            return;
        }

        // Get the actual device spec (SDL3 might adjust it)
        if (!SDL_GetAudioDeviceFormat(sound_state.device, &sound_state.device_spec, NULL)) {
            log_warn("Failed to get audio device format, using desired spec");
            sound_state.device_spec = desired;
        }
        
        SDL_zero(sound_state.active_streams);
        sound_state.music_main_buffer = NULL;
        sound_state.music_main_length = 0;
        sound_state.music_ambient_buffer = NULL;
        sound_state.music_ambient_length = 0;
        
        SDL_ResumeAudioDevice(sound_state.device);
        log_info("Audio device opened (freq=%d, channels=%d, format=0x%x)",
                 sound_state.device_spec.freq,
                 sound_state.device_spec.channels,
                 sound_state.device_spec.format);
    } else {
        log_debug("Skipping audio device init: enabled=%d, device=%u", g_sound_config.enabled, sound_state.device);
    }
}

void sdl_sound_shutdown(void)
{
    sdl_music_stop_main();
    sdl_music_stop_ambient();
    
    if (sound_state.music_main_buffer) {
        SDL_free(sound_state.music_main_buffer);
        sound_state.music_main_buffer = NULL;
    }
    if (sound_state.music_ambient_buffer) {
        SDL_free(sound_state.music_ambient_buffer);
        sound_state.music_ambient_buffer = NULL;
    }
    
    sdl_sound_clear_streams();
    if (sound_state.device) {
        SDL_CloseAudioDevice(sound_state.device);
        sound_state.device = 0;
    }
}

static void sdl_music_play(const char* path, SDL_AudioStream** stream_ptr, float volume, bool loop,
                           Uint8** buffer_ptr, Uint32* length_ptr, SDL_AudioSpec* spec_ptr)
{
    if (!path || !path[0] || !stream_ptr) {
        return;
    }

    if (*stream_ptr) {
        sdl_music_stop(stream_ptr);
    }

    // Free old buffer if exists
    if (buffer_ptr && *buffer_ptr) {
        SDL_free(*buffer_ptr);
        *buffer_ptr = NULL;
        if (length_ptr) *length_ptr = 0;
    }

    SDL_AudioSpec wav_spec;
    Uint8* wav_buffer = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV(path, &wav_spec, &wav_buffer, &wav_length)) {
        log_debug("Failed to load music file '%s': %s", path, SDL_GetError());
        return;
    }

    // Store the WAV data for looping
    if (loop && buffer_ptr && length_ptr && spec_ptr) {
        *buffer_ptr = wav_buffer;
        *length_ptr = wav_length;
        *spec_ptr = wav_spec;
        log_debug("Stored music buffer for looping: %d bytes", wav_length);
    }

    SDL_AudioStream* playback_stream = SDL_CreateAudioStream(&wav_spec, &sound_state.device_spec);
    if (!playback_stream) {
        log_warn("Failed to create music stream: %s", SDL_GetError());
        if (!loop || !buffer_ptr) SDL_free(wav_buffer);
        return;
    }

    bool success = SDL_PutAudioStreamData(playback_stream, wav_buffer, wav_length) &&
                   SDL_FlushAudioStream(playback_stream);
    
    if (!loop) {
        SDL_free(wav_buffer);
    }

    if (!success) {
        log_warn("Failed to queue music data: %s", SDL_GetError());
        SDL_DestroyAudioStream(playback_stream);
        if (loop && buffer_ptr && *buffer_ptr) {
            SDL_free(*buffer_ptr);
            *buffer_ptr = NULL;
        }
        return;
    }

    if (!SDL_SetAudioStreamGain(playback_stream, volume)) {
        log_warn("Failed to set music stream gain: %s", SDL_GetError());
    }

    if (!SDL_BindAudioStream(sound_state.device, playback_stream)) {
        log_warn("Failed to bind music stream: %s", SDL_GetError());
        SDL_DestroyAudioStream(playback_stream);
        if (loop && buffer_ptr && *buffer_ptr) {
            SDL_free(*buffer_ptr);
            *buffer_ptr = NULL;
        }
        return;
    }

    *stream_ptr = playback_stream;
    log_debug("Music stream created and bound successfully");
}

void sdl_music_stop(SDL_AudioStream** stream_ptr)
{
    if (stream_ptr && *stream_ptr) {
        SDL_UnbindAudioStream(*stream_ptr);
        SDL_DestroyAudioStream(*stream_ptr);
        *stream_ptr = NULL;
    }
}

void sdl_music_play_main(void)
{
    if (sound_state.music_main_enabled) {
        log_debug("Starting main menu music: %s", sound_state.music_main_path);
        sdl_music_play(sound_state.music_main_path, &sound_state.music_main_stream, 
                      sound_state.music_main_volume, true,
                      &sound_state.music_main_buffer, &sound_state.music_main_length,
                      &sound_state.music_main_spec);
    }
}

void sdl_music_play_ambient(void)
{
    if (sound_state.music_ambient_enabled) {
        log_debug("Starting ambient music: %s", sound_state.music_ambient_path);
        sdl_music_play(sound_state.music_ambient_path, &sound_state.music_ambient_stream, 
                      sound_state.music_ambient_volume, true,
                      &sound_state.music_ambient_buffer, &sound_state.music_ambient_length,
                      &sound_state.music_ambient_spec);
    }
}

void sdl_music_stop_main(void)
{
    sdl_music_stop(&sound_state.music_main_stream);
}

void sdl_music_stop_ambient(void)
{
    sdl_music_stop(&sound_state.music_ambient_stream);
}

void sdl_music_update(void)
{
    if (!sound_state.device) {
        return;
    }

    // Check and refill main music stream if needed
    if (sound_state.music_main_stream && sound_state.music_main_buffer) {
        int queued = SDL_GetAudioStreamQueued(sound_state.music_main_stream);
        
        // Refill if stream is getting low (less than 0.5 seconds of audio)
        if (queued < (sound_state.device_spec.freq * sound_state.device_spec.channels * 2)) {
            if (!SDL_PutAudioStreamData(sound_state.music_main_stream, 
                                       sound_state.music_main_buffer, 
                                       sound_state.music_main_length)) {
                log_debug("Failed to refill main music stream: %s", SDL_GetError());
            }
        }
    }

    // Check and refill ambient music stream if needed
    if (sound_state.music_ambient_stream && sound_state.music_ambient_buffer) {
        int queued = SDL_GetAudioStreamQueued(sound_state.music_ambient_stream);
        
        // Refill if stream is getting low (less than 0.5 seconds of audio)
        if (queued < (sound_state.device_spec.freq * sound_state.device_spec.channels * 2)) {
            if (!SDL_PutAudioStreamData(sound_state.music_ambient_stream, 
                                       sound_state.music_ambient_buffer, 
                                       sound_state.music_ambient_length)) {
                log_debug("Failed to refill ambient music stream: %s", SDL_GetError());
            }
        }
    }
}

void sdl_music_update_volumes(void)
{
    /* Update volume for currently playing main music */
    if (sound_state.music_main_stream) {
        if (!SDL_SetAudioStreamGain(sound_state.music_main_stream, sound_state.music_main_volume)) {
            log_debug("Failed to update main music volume: %s", SDL_GetError());
        }
    }
    
    /* Update volume for currently playing ambient music */
    if (sound_state.music_ambient_stream) {
        if (!SDL_SetAudioStreamGain(sound_state.music_ambient_stream, sound_state.music_ambient_volume)) {
            log_debug("Failed to update ambient music volume: %s", SDL_GetError());
        }
    }
}

void sdl_sound_handle(int sound_idx)
{
    if (sound_idx < 0 || sound_idx >= MSG_MAX) {
        return;
    }
    if (!sound_state.device) {
        return;
    }

    // Check if sound group is enabled
    if (!is_sound_enabled(sound_idx)) {
        return;
    }

    log_debug("sdl_sound_handle: Playing sound idx=%d", sound_idx);

    int sample_count = sound_state.bank.sound_counts[sound_idx];
    if (sample_count <= 0) {
        return;
    }

    int sample_idx = (sample_count > 1) ? Rand_div(sample_count) : 0;
    const char* sample_path = sound_state.bank.sound_files[sound_idx][sample_idx];
    if (!sample_path[0]) {
        return;
    }

    SDL_AudioSpec wav_spec;
    Uint8* wav_buffer = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV(sample_path, &wav_spec, &wav_buffer, &wav_length)) {
        log_debug("Failed to load sound sample '%s': %s", sample_path, SDL_GetError());
        return;
    }

    SDL_AudioStream* playback_stream = SDL_CreateAudioStream(&wav_spec, &sound_state.device_spec);
    if (!playback_stream) {
        log_warn("Failed to create playback stream: %s", SDL_GetError());
        SDL_free(wav_buffer);
        return;
    }

    bool success = SDL_PutAudioStreamData(playback_stream, wav_buffer, wav_length) &&
                   SDL_FlushAudioStream(playback_stream);
    SDL_free(wav_buffer);

    if (!success) {
        log_warn("Failed to queue sound data: %s", SDL_GetError());
        SDL_DestroyAudioStream(playback_stream);
        return;
    }

    // Apply volume based on sound category
    float volume = get_sound_volume(sound_idx);
    if (!SDL_SetAudioStreamGain(playback_stream, volume)) {
        log_warn("Failed to set stream gain: %s", SDL_GetError());
    }

    if (!SDL_BindAudioStream(sound_state.device, playback_stream)) {
        log_warn("Failed to bind sound stream: %s", SDL_GetError());
        SDL_DestroyAudioStream(playback_stream);
        return;
    }

    sdl_sound_track_stream(playback_stream);
}

void sdl_init_sounds(void)
{
    sdl_sound_reload();
}

struct sound_config* sdl_sound_get_config(void)
{
    return &g_sound_config;
}

void sdl_sound_save_config(void)
{
    /* Update sound_state from global config */
    sound_state.enable_combat = g_sound_config.enable_combat;
    sound_state.enable_inventory = g_sound_config.enable_inventory;
    sound_state.enable_walk = g_sound_config.enable_walk;
    sound_state.enable_doors = g_sound_config.enable_doors;
    sound_state.volume_combat = g_sound_config.volume_combat;
    sound_state.volume_inventory = g_sound_config.volume_inventory;
    sound_state.volume_walk = g_sound_config.volume_walk;
    sound_state.volume_doors = g_sound_config.volume_doors;
    sound_state.volume_other = g_sound_config.volume_other;
    sound_state.music_main_enabled = g_sound_config.music_main_enabled;
    sound_state.music_ambient_enabled = g_sound_config.music_ambient_enabled;
    sound_state.music_main_volume = g_sound_config.music_main_volume;
    sound_state.music_ambient_volume = g_sound_config.music_ambient_volume;
    
    /* Apply volume changes to currently playing music */
    sdl_music_update_volumes();
    
    /* Save to disk */
    sound_config_save(g_sound_config_path, &g_sound_config);
    log_debug("Sound configuration saved to %s", g_sound_config_path);
}
