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
} sound_state;

static void sdl_sound_reset_bank(void);
static bool sdl_sound_load_from_config(const struct sound_config* config);
static bool sdl_sound_scan_folder(const char* folder_path, char files[][SDL_SOUND_NAME_LEN], int* file_count, int max_files);
static char* sdl_sound_trim(char* text);
static bool sdl_sound_is_audio_file(const char* filename);
static void sdl_sound_build_path(const char* base_path, char* dst, size_t dst_len);
static void sdl_sound_clear_streams(void);
static bool sdl_sound_track_stream(SDL_AudioStream* stream);
static void sdl_sound_destroy_stream(SDL_AudioStream** stream);

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
    return (SDL_strcasecmp(ext, ".wav") == 0 || SDL_strcasecmp(ext, ".ogg") == 0);
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
    } else {
        const char* anchor = ANGBAND_DIR_XTRA;
        if (!anchor || !anchor[0]) {
            dst[0] = '\0';
            return;
        }
        path_build(dst, dst_len, anchor, base_path);
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

static char* sdl_sound_trim(char* text)
{
    char* start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    char* end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';
    return start;
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
    
    // Load sound configuration from sound.json
    static struct sound_config sound_cfg;
    
    char sound_config_path[1024];
    if (ANGBAND_DIR_USER && ANGBAND_DIR_USER[0]) {
        path_build(sound_config_path, sizeof(sound_config_path), ANGBAND_DIR_USER, "sound.json");
    } else {
        SDL_strlcpy(sound_config_path, "sound.json", sizeof(sound_config_path));
    }
    
    sound_config_load(sound_config_path, &sound_cfg);
    sound_state.bank_loaded = sdl_sound_load_from_config(&sound_cfg);
    
    // Open audio device if not already open and sound is enabled
    if (sound_cfg.enabled && !sound_state.device) {
        SDL_AudioSpec desired;
        SDL_zero(desired);
        desired.freq = sound_cfg.sample_rate;
        desired.format = sdl_sound_parse_format(sound_cfg.format);
        desired.channels = sound_cfg.channels;

        sound_state.device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (!sound_state.device) {
            log_warn("Failed to open audio device: %s", SDL_GetError());
            return;
        }

        sound_state.device_spec = desired;
        SDL_zero(sound_state.active_streams);
        SDL_ResumeAudioDevice(sound_state.device);
        log_info("Audio device opened (freq=%d, channels=%d, format=0x%x)",
                 sound_state.device_spec.freq,
                 sound_state.device_spec.channels,
                 sound_state.device_spec.format);
    }
}

void sdl_sound_shutdown(void)
{
    sdl_sound_clear_streams();
    if (sound_state.device) {
        SDL_CloseAudioDevice(sound_state.device);
        sound_state.device = 0;
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
