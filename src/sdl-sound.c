#include "sdl-sound.h"

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <SDL3/SDL.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define SDL_SOUND_MAX_VARIANTS 16
#define SDL_SOUND_NAME_LEN 64
#define SDL_SOUND_MAX_PATHS 12

typedef enum {
    SOUND_SECTION_NONE,
    SOUND_SECTION_AUDIO,
    SOUND_SECTION_AUDIO_PATHS,
    SOUND_SECTION_SOUND,
} sound_section;

typedef struct {
    char search_paths[SDL_SOUND_MAX_PATHS][128];
    int search_path_count;
    char extension[16];
    int sample_rate;
    int channels;
    SDL_AudioFormat format;
} sdl_sound_settings;

static struct {
    SDL_AudioDeviceID device;
    SDL_AudioStream* stream;
    SDL_AudioSpec device_spec;
    sdl_sound_settings settings;
    char sound_files[MSG_MAX][SDL_SOUND_MAX_VARIANTS][SDL_SOUND_NAME_LEN];
    int sound_counts[MSG_MAX];
    bool bank_loaded;
} sound_state;

static void sdl_sound_reset_settings(void);
static void sdl_sound_reset_bank(void);
static bool sdl_sound_parse_config(void);
static void sdl_sound_process_audio_entry(const char* key, const char* value);
static void sdl_sound_process_sound_entry(const char* key, char* value);
static void sdl_sound_add_search_path(const char* path);
static char* sdl_sound_trim(char* text);
static int sdl_sound_lookup_event(const char* name);
static bool sdl_sound_is_absolute_path(const char* path);
static void sdl_sound_build_directory(const char* base_path, char* dst, size_t dst_len);
static void sdl_sound_build_sample_path(const char* base_path, const char* sample, char* dst, size_t dst_len);

static void sdl_sound_reset_settings(void)
{
    sound_state.settings.search_path_count = 1;
    SDL_zero(sound_state.settings.search_paths);
    SDL_strlcpy(sound_state.settings.search_paths[0], "sound", sizeof(sound_state.settings.search_paths[0]));
    SDL_strlcpy(sound_state.settings.extension, "wav", sizeof(sound_state.settings.extension));
    sound_state.settings.sample_rate = 22050;
    sound_state.settings.channels = 2;
    sound_state.settings.format = SDL_AUDIO_S16;
}

static void sdl_sound_reset_bank(void)
{
    for (int i = 0; i < MSG_MAX; i++) {
        sound_state.sound_counts[i] = 0;
        for (int j = 0; j < SDL_SOUND_MAX_VARIANTS; j++) {
            sound_state.sound_files[i][j][0] = '\0';
        }
    }
}

static void sdl_sound_add_search_path(const char* path)
{
    if (!path || !path[0]) {
        return;
    }
    if (sound_state.settings.search_path_count >= SDL_SOUND_MAX_PATHS) {
        log_warn("Too many sound search paths configured (max=%d)", SDL_SOUND_MAX_PATHS);
        return;
    }
    SDL_strlcpy(sound_state.settings.search_paths[sound_state.settings.search_path_count],
                path,
                sizeof(sound_state.settings.search_paths[0]));
    sound_state.settings.search_path_count++;
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

static int sdl_sound_lookup_event(const char* name)
{
    extern const cptr angband_sound_name[];
    for (int i = 0; i < MSG_MAX; i++) {
        if (angband_sound_name[i] && streq(angband_sound_name[i], name)) {
            return i;
        }
    }
    return -1;
}

static bool sdl_sound_is_absolute_path(const char* path)
{
    if (!path || !path[0]) {
        return false;
    }
#ifdef _WIN32
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') {
        return true;
    }
#endif
    return path[0] == '/' || path[0] == '\\';
}

static void sdl_sound_build_directory(const char* base_path, char* dst, size_t dst_len)
{
    if (base_path && sdl_sound_is_absolute_path(base_path)) {
        SDL_strlcpy(dst, base_path, dst_len);
        return;
    }

    const char* anchor = ANGBAND_DIR_XTRA;
    if (!anchor || !anchor[0]) {
        dst[0] = '\0';
        return;
    }

    const char* rel = (base_path && base_path[0]) ? base_path : "sound";
    path_build(dst, dst_len, anchor, rel);
}

static void sdl_sound_build_sample_path(const char* base_path, const char* sample, char* dst, size_t dst_len)
{
    if (!sample || !sample[0]) {
        dst[0] = '\0';
        return;
    }

    bool has_ext = (strchr(sample, '.') != NULL);

    if (sdl_sound_is_absolute_path(sample)) {
        if (has_ext || !sound_state.settings.extension[0]) {
            SDL_strlcpy(dst, sample, dst_len);
        } else {
            strnfmt(dst, dst_len, "%s.%s", sample, sound_state.settings.extension);
        }
        return;
    }

    char directory[1024];
    sdl_sound_build_directory(base_path, directory, sizeof(directory));

    if (!directory[0]) {
        dst[0] = '\0';
        return;
    }

    if (has_ext || !sound_state.settings.extension[0]) {
        strnfmt(dst, dst_len, "%s/%s", directory, sample);
    } else {
        strnfmt(dst, dst_len, "%s/%s.%s", directory, sample, sound_state.settings.extension);
    }
}

static void sdl_sound_process_audio_entry(const char* key, const char* value)
{
    if (SDL_strcasecmp(key, "base_path") == 0) {
        if (value && value[0]) {
            sound_state.settings.search_path_count = 1;
            SDL_strlcpy(sound_state.settings.search_paths[0], value,
                        sizeof(sound_state.settings.search_paths[0]));
        }
    } else if (SDL_strcasecmp(key, "path") == 0) {
        sdl_sound_add_search_path(value);
    } else if (SDL_strcasecmp(key, "paths") == 0) {
        if (value && value[0]) {
            char buffer[256];
            SDL_strlcpy(buffer, value, sizeof(buffer));
            char* token = buffer;
            while (*token) {
                char* segment = token;
                while (*token && *token != ',' && *token != ';') token++;
                char saved = *token;
                *token = '\0';
                char* trimmed = sdl_sound_trim(segment);
                if (*trimmed) {
                    sdl_sound_add_search_path(trimmed);
                }
                if (!saved) {
                    break;
                }
                token++;
            }
        }
    } else if (SDL_strcasecmp(key, "extension") == 0) {
        const char* v = value;
        if (v[0] == '.') v++;
        SDL_strlcpy(sound_state.settings.extension, v, sizeof(sound_state.settings.extension));
    } else if (SDL_strcasecmp(key, "sample_rate") == 0) {
        long rate = strtol(value, NULL, 10);
        if (rate > 0 && rate < 192001) {
            sound_state.settings.sample_rate = (int)rate;
        }
    } else if (SDL_strcasecmp(key, "channels") == 0) {
        long channels = strtol(value, NULL, 10);
        if (channels == 1 || channels == 2) {
            sound_state.settings.channels = (int)channels;
        }
    } else if (SDL_strcasecmp(key, "format") == 0) {
        char lowered[16];
        SDL_strlcpy(lowered, value, sizeof(lowered));
        for (char* p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (streq(lowered, "s8")) sound_state.settings.format = SDL_AUDIO_S8;
        else if (streq(lowered, "u8")) sound_state.settings.format = SDL_AUDIO_U8;
        else if (streq(lowered, "s16")) sound_state.settings.format = SDL_AUDIO_S16;
        else if (streq(lowered, "s32")) sound_state.settings.format = SDL_AUDIO_S32;
        else if (streq(lowered, "f32")) sound_state.settings.format = SDL_AUDIO_F32;
    }
}

static void sdl_sound_process_sound_entry(const char* key, char* value)
{
    int idx = sdl_sound_lookup_event(key);
    if (idx < 0) {
        log_debug("Unknown sound event '%s' in sound.cfg", key);
        return;
    }

    int count = 0;
    char* cursor = value;
    while (*cursor && count < SDL_SOUND_MAX_VARIANTS) {
        while (*cursor && isspace((unsigned char)*cursor)) cursor++;
        if (!*cursor) break;
        char* token_end = cursor;
        while (*token_end && !isspace((unsigned char)*token_end)) token_end++;
        char save = *token_end;
        *token_end = '\0';
        SDL_strlcpy(sound_state.sound_files[idx][count], cursor, SDL_SOUND_NAME_LEN);
        count++;
        *token_end = save;
        cursor = token_end;
    }

    if (*cursor && count >= SDL_SOUND_MAX_VARIANTS) {
        log_warn("Sound event '%s' truncated to %d variants", key, SDL_SOUND_MAX_VARIANTS);
    }

    sound_state.sound_counts[idx] = count;
    if (count > 0) {
        log_debug("Sound event '%s' (idx=%d): %d variant(s)", key, idx, count);
    }
}

static bool sdl_sound_parse_config(void)
{
    if (!ANGBAND_DIR_XTRA || !ANGBAND_DIR_XTRA[0]) {
        log_warn("ANGBAND_DIR_XTRA is not configured; skipping sound.cfg");
        return false;
    }

    char sound_dir[1024];
    char cfg_path[1024];
    path_build(sound_dir, sizeof(sound_dir), ANGBAND_DIR_XTRA, "sound");
    path_build(cfg_path, sizeof(cfg_path), sound_dir, "sound.cfg");

    SDL_IOStream* file = sdl_fopen(cfg_path, "r");
    if (!file) {
        log_warn("Could not open sound config file: %s", cfg_path);
        return false;
    }

    char line[256];
    sound_section section = SOUND_SECTION_NONE;

    while (sdl_fgets(file, line, sizeof(line)) == 0) {
        char* trimmed = sdl_sound_trim(line);
        if (*trimmed == '\0' || *trimmed == '#') {
            continue;
        }

        if (*trimmed == '[') {
            if (strstr(trimmed, "[Audio]")) {
                section = SOUND_SECTION_AUDIO;
            } else if (strstr(trimmed, "[AudioPaths]")) {
                section = SOUND_SECTION_AUDIO_PATHS;
            } else if (strstr(trimmed, "[Sound]")) {
                section = SOUND_SECTION_SOUND;
            } else {
                section = SOUND_SECTION_NONE;
            }
            continue;
        }

        char* equals = strchr(trimmed, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char* key = sdl_sound_trim(trimmed);
        char* value = sdl_sound_trim(equals + 1);

        if (section == SOUND_SECTION_AUDIO) {
            sdl_sound_process_audio_entry(key, value);
        } else if (section == SOUND_SECTION_AUDIO_PATHS) {
            sdl_sound_add_search_path(value);
        } else if (section == SOUND_SECTION_SOUND) {
            sdl_sound_process_sound_entry(key, value);
        }
    }

    sdl_fclose(file);
    log_info("Sound configuration loaded from %s", cfg_path);
    return true;
}

bool sdl_sound_initialize(void)
{
    if (!sound_state.bank_loaded) {
        sdl_sound_reset_settings();
        sdl_sound_reset_bank();
        sound_state.bank_loaded = sdl_sound_parse_config();
    }

    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = sound_state.settings.sample_rate;
    desired.format = sound_state.settings.format;
    desired.channels = sound_state.settings.channels;

    sound_state.device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (!sound_state.device) {
        log_warn("Failed to open audio device: %s", SDL_GetError());
        return false;
    }

    sound_state.device_spec = desired;
    sound_state.stream = SDL_CreateAudioStream(&desired, &desired);
    if (!sound_state.stream) {
        log_warn("Failed to create audio stream: %s", SDL_GetError());
        SDL_CloseAudioDevice(sound_state.device);
        sound_state.device = 0;
        return false;
    }

    SDL_BindAudioStream(sound_state.device, sound_state.stream);
    SDL_ResumeAudioDevice(sound_state.device);
    log_info("Audio device opened (freq=%d, channels=%d, format=0x%x)",
             sound_state.device_spec.freq,
             sound_state.device_spec.channels,
             sound_state.device_spec.format);
    return true;
}

void sdl_sound_reload(void)
{
    sdl_sound_reset_settings();
    sdl_sound_reset_bank();
    sound_state.bank_loaded = sdl_sound_parse_config();
}

void sdl_sound_shutdown(void)
{
    if (sound_state.stream) {
        SDL_DestroyAudioStream(sound_state.stream);
        sound_state.stream = NULL;
    }
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
    if (!sound_state.device || !sound_state.stream) {
        return;
    }

    int sample_count = sound_state.sound_counts[sound_idx];
    if (sample_count <= 0) {
        return;
    }

    int sample_idx = (sample_count > 1) ? Rand_div(sample_count) : 0;
    const char* sample = sound_state.sound_files[sound_idx][sample_idx];
    if (!sample[0]) {
        return;
    }

    SDL_AudioSpec wav_spec;
    Uint8* wav_buffer = NULL;
    Uint32 wav_length = 0;
    bool loaded = false;
    char sample_path[1024];

    if (sdl_sound_is_absolute_path(sample)) {
        sdl_sound_build_sample_path(NULL, sample, sample_path, sizeof(sample_path));
        if (sample_path[0] && SDL_LoadWAV(sample_path, &wav_spec, &wav_buffer, &wav_length)) {
            loaded = true;
        }
    } else {
        int path_count = sound_state.settings.search_path_count;
        if (path_count <= 0) path_count = 1;
        for (int i = 0; i < path_count && !loaded; i++) {
            const char* base_path = (i < sound_state.settings.search_path_count) ?
                sound_state.settings.search_paths[i] : "sound";
            sdl_sound_build_sample_path(base_path, sample, sample_path, sizeof(sample_path));
            if (!sample_path[0]) continue;
            if (SDL_LoadWAV(sample_path, &wav_spec, &wav_buffer, &wav_length)) {
                loaded = true;
                break;
            }
        }
    }

    if (!loaded) {
        log_debug("Failed to load sound sample '%s' from configured paths", sample);
        return;
    }

    SDL_AudioStream* convert_stream = SDL_CreateAudioStream(&wav_spec, &sound_state.device_spec);
    if (convert_stream) {
        if (SDL_PutAudioStreamData(convert_stream, wav_buffer, wav_length) &&
            SDL_FlushAudioStream(convert_stream)) {
            int available = SDL_GetAudioStreamAvailable(convert_stream);
            if (available > 0) {
                Uint8* converted = SDL_malloc(available);
                if (converted) {
                    int got = SDL_GetAudioStreamData(convert_stream, converted, available);
                    if (got > 0) {
                        SDL_PutAudioStreamData(sound_state.stream, converted, got);
                    }
                    SDL_free(converted);
                }
            }
        }
        SDL_DestroyAudioStream(convert_stream);
    }
    SDL_free(wav_buffer);
}

void sdl_init_sounds(void)
{
    sdl_sound_reload();
}
