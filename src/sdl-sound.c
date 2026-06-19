#include "sdl-sound.h"

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "sound-config.h"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define SDL_SOUND_MAX_VARIANTS 64
#define SDL_SOUND_NAME_LEN 256
#define SDL_SOUND_MAX_ACTIVE_TRACKS 16
#define SDL_SOUND_MAX_MUSIC_CACHE 8

typedef struct {
    char path[1024];
    MIX_Audio* audio;
} music_audio_cache_entry;

typedef struct {
    char sound_files[MSG_MAX][SDL_SOUND_MAX_VARIANTS][SDL_SOUND_NAME_LEN];
    MIX_Audio* sound_audio[MSG_MAX][SDL_SOUND_MAX_VARIANTS];
    int sound_counts[MSG_MAX];
} sound_bank;

typedef struct {
    char (*files)[SDL_SOUND_NAME_LEN];
    int* file_count;
    int max_files;
} sound_scan_ctx;

static struct {
    MIX_Mixer* mixer;
    SDL_AudioSpec mixer_spec;
    sound_bank bank;
    bool bank_loaded;
    bool mixer_initialized;
    bool mixer_paused;
    bool enable_combat;
    bool enable_inventory;
    bool enable_walk;
    bool enable_doors;
    bool enable_monster_hits;
    bool enable_traps;
    float volume_combat;
    float volume_inventory;
    float volume_walk;
    float volume_doors;
    float volume_monster_hits;
    float volume_traps;
    float volume_other;
    MIX_Track* sfx_tracks[SDL_SOUND_MAX_ACTIVE_TRACKS];
    int next_sfx_track;
    MIX_Track* music_main_track;
    MIX_Track* music_menu_track;
    MIX_Track* music_ambient_track;
    char music_main_path[1024];
    char music_main_full_path[1024];
    char music_ambient_path[1024];
    char music_death_path[1024];
    char music_main_current_path[1024];
    float music_main_volume;
    float music_ambient_volume;
    bool music_main_enabled;
    bool music_ambient_enabled;
    music_audio_cache_entry music_cache[SDL_SOUND_MAX_MUSIC_CACHE];
} sound_state;

static struct sound_config g_sound_config;
static char g_sound_config_path[1024];
static bool g_music_force_main_on_next_welcome = false;
static bool g_sound_loaded_once = false;
static SDL_Mutex* g_sound_mutex = NULL;
static Uint64 g_sound_generation = 1;
static Uint32 g_sound_diagnostic_event = (Uint32)-1;

typedef struct {
    int sound_idx;
    int sample_idx;
    Uint64 generation;
    Uint64 due_ns;
} delayed_sound_request;

static void sdl_sound_reset_bank(void);
static void sdl_sound_destroy_cached_audio(void);
static bool sdl_sound_load_from_config(const struct sound_config* config);
static bool sdl_sound_scan_folder(const char* folder_path, char files[][SDL_SOUND_NAME_LEN],
    int* file_count, int max_files);
static bool sdl_sound_is_audio_file(const char* filename);
static void sdl_sound_build_path(const char* base_path, char* dst, size_t dst_len);
static bool sdl_sound_has_any_events(const struct sound_config* config);
static bool sdl_sound_load_default_config(struct sound_config* defaults);
static void sdl_sound_fill_missing_events_from_defaults(struct sound_config* config);
static SDL_AudioFormat sdl_sound_parse_format(const char* format_str);
static bool sdl_sound_path_exists(const char* path);
static bool sdl_sound_make_alternate_music_path(const char* path, char* alternate,
    size_t alternate_len);
static bool sdl_sound_resolve_music_path(const char* path, char* resolved,
    size_t resolved_len);
static bool sdl_sound_create_track_pool(void);
static bool sdl_sound_ensure_mixer(void);
static void sdl_sound_destroy_mixer(void);
static MIX_Audio* sdl_sound_get_sample_audio(int sound_idx, int sample_idx);
static MIX_Audio* sdl_sound_get_music_audio(const char* path);
static MIX_Track* sdl_sound_acquire_sfx_track(bool quiet);
static bool sdl_sound_play_track_audio(MIX_Track* track, MIX_Audio* audio, float gain,
    int loops, bool quiet);
static bool sdl_sound_play_sample_locked(int sound_idx, int sample_idx,
    bool quiet);
static void sdl_sound_preload_event(int sound_idx);
static void sdl_music_stop_track(MIX_Track* track);
static void sdl_music_stop_title_track(void);
static bool sdl_music_play_title_track(const char* primary_path,
    const char* fallback_path, const char* label);
static bool sdl_music_play_with_fallback(const char* primary_path,
    const char* fallback_path, MIX_Track* track, float gain, int loops,
    const char* label, char* resolved_path, size_t resolved_len);

static SDL_EnumerationResult SDLCALL sdl_sound_scan_enum_cb(void* userdata,
    const char* dirname, const char* fname)
{
    sound_scan_ctx* ctx = (sound_scan_ctx*)userdata;
    if (!ctx || !ctx->files || !ctx->file_count || !dirname || !fname) {
        return SDL_ENUM_CONTINUE;
    }

    if (!fname[0] || streq(fname, ".") || streq(fname, "..")) {
        return SDL_ENUM_CONTINUE;
    }

    if (!sdl_sound_is_audio_file(fname)) {
        return SDL_ENUM_CONTINUE;
    }

    int idx = *ctx->file_count;
    if (idx >= ctx->max_files) {
        return SDL_ENUM_SUCCESS;
    }

    char candidate_path[1024];
    candidate_path[0] = '\0';
    SDL_strlcpy(candidate_path, dirname, sizeof(candidate_path));
    SDL_strlcat(candidate_path, fname, sizeof(candidate_path));

    SDL_strlcpy(ctx->files[idx], candidate_path, SDL_SOUND_NAME_LEN);
    (*ctx->file_count)++;
    return SDL_ENUM_CONTINUE;
}

static bool sound_is_monster_hit(int sound_idx)
{
    return sound_idx == MSG_MONSTER_ATTACK ||
        sound_idx == MSG_MONSTER_ATTACK_RANGED ||
        sound_idx == MSG_MONSTER_ATTACK_BREATH;
}

static bool sound_is_combat(int sound_idx)
{
    if (sound_idx == MSG_HIT || sound_idx == MSG_SHOOT || sound_idx == MSG_DIG ||
        (sound_idx >= MSG_WEAPON_SLASH_LIGHT && sound_idx <= MSG_WEAPON_UNARMED) ||
        sound_idx == MSG_WEAPON_SLASH_MEDIUM || sound_idx == MSG_MISS ||
        sound_idx == MSG_KILL) {
        return true;
    }

    return false;
}

static bool sound_is_inventory(int sound_idx)
{
    if (sound_idx == MSG_DROP || sound_idx == MSG_QUAFF || sound_idx == MSG_ZAP ||
        sound_idx == MSG_EAT || sound_idx == MSG_PICK || sound_idx == MSG_ARMOR ||
        sound_idx == MSG_TORCH_LIGHT ||
        (sound_idx >= MSG_EQUIP_SWORD && sound_idx <= MSG_UNEQUIP_JEWELRY) ||
        (sound_idx >= MSG_DROP_GLASS && sound_idx <= MSG_ACTIVATE)) {
        return true;
    }

    return false;
}

static bool sound_is_trap(int sound_idx)
{
    return sound_idx == MSG_TRAP_GAS || sound_idx == MSG_TRAP_NEEDLE ||
        sound_idx == MSG_TRAP_FIRE;
}

static bool sound_is_door(int sound_idx)
{
    if (sound_idx == MSG_OPENDOOR || sound_idx == MSG_SHUTDOOR ||
        sound_idx == MSG_BASHDOOR || sound_idx == MSG_BASHDOOR_FAIL ||
        sound_idx == MSG_HITWALL || sound_idx == MSG_CHEST_OPEN ||
        sound_idx == MSG_NOTHING_TO_OPEN || sound_idx == MSG_LOCKPICK_FAIL) {
        return true;
    }

    return false;
}

static bool is_sound_enabled(int sound_idx)
{
    if (sound_is_combat(sound_idx)) {
        return sound_state.enable_combat;
    }

    if (sound_is_monster_hit(sound_idx)) {
        return sound_state.enable_monster_hits;
    }

    if (sound_is_inventory(sound_idx)) {
        return sound_state.enable_inventory;
    }

    if (sound_idx == MSG_WALK) {
        return sound_state.enable_walk;
    }

    if (sound_is_trap(sound_idx)) {
        return sound_state.enable_traps;
    }

    if (sound_is_door(sound_idx)) {
        return sound_state.enable_doors;
    }

    return true;
}

static float get_sound_volume(int sound_idx)
{
    if (sound_is_combat(sound_idx)) {
        return sound_state.volume_combat;
    }

    if (sound_is_monster_hit(sound_idx)) {
        return sound_state.volume_monster_hits;
    }

    if (sound_is_inventory(sound_idx)) {
        return sound_state.volume_inventory;
    }

    if (sound_idx == MSG_WALK) {
        return sound_state.volume_walk;
    }

    if (sound_is_trap(sound_idx)) {
        return sound_state.volume_traps;
    }

    if (sound_is_door(sound_idx)) {
        return sound_state.volume_doors;
    }

    return sound_state.volume_other;
}

static void sdl_sound_reset_bank(void)
{
    sound_state.bank_loaded = false;
    for (int i = 0; i < MSG_MAX; i++) {
        sound_state.bank.sound_counts[i] = 0;
        for (int j = 0; j < SDL_SOUND_MAX_VARIANTS; j++) {
            sound_state.bank.sound_files[i][j][0] = '\0';
            sound_state.bank.sound_audio[i][j] = NULL;
        }
    }
}

static void sdl_sound_destroy_cached_audio(void)
{
    for (int i = 0; i < MSG_MAX; i++) {
        for (int j = 0; j < SDL_SOUND_MAX_VARIANTS; j++) {
            if (sound_state.bank.sound_audio[i][j]) {
                MIX_DestroyAudio(sound_state.bank.sound_audio[i][j]);
                sound_state.bank.sound_audio[i][j] = NULL;
            }
        }
    }

    for (int i = 0; i < SDL_SOUND_MAX_MUSIC_CACHE; i++) {
        if (sound_state.music_cache[i].audio) {
            MIX_DestroyAudio(sound_state.music_cache[i].audio);
            sound_state.music_cache[i].audio = NULL;
        }
        sound_state.music_cache[i].path[0] = '\0';
    }
}

static bool sdl_sound_is_audio_file(const char* filename)
{
    if (!filename || !filename[0]) {
        return false;
    }

    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return false;
    }

    return SDL_strcasecmp(ext, ".wav") == 0 ||
        SDL_strcasecmp(ext, ".ogg") == 0 ||
        SDL_strcasecmp(ext, ".oga") == 0 ||
        SDL_strcasecmp(ext, ".opus") == 0;
}

#ifdef __ANDROID__
/* On Android the read-only game data lives inside the APK asset tree. SDL can
 * *open* an asset through a plain relative path (SDL_IOFromFile falls back to
 * the asset manager), which is why streamed music works. However it can only
 * *enumerate* an asset directory when the path uses the explicit "assets://"
 * scheme: for a relative path SDL_EnumerateDirectory rewrites it against the
 * internal-storage root and never falls back to the APK, so the per-event
 * folder scan finds zero samples and no sound effects play. Rewrite
 * asset-relative paths to the asset URI so both scanning and loading reach the
 * APK assets. */
static void sdl_sound_to_asset_uri(char* path, size_t path_len)
{
    if (!path || !path[0]) {
        return;
    }

    /* Leave explicit URIs and absolute real-filesystem paths untouched. */
    if (SDL_strncmp(path, "assets://", 9) == 0 || path[0] == '/') {
        return;
    }

    const char* rel = path;
    if (rel[0] == '.' && (rel[1] == '/' || rel[1] == '\\')) {
        rel += 2;
    }
    while (*rel == '/' || *rel == '\\') {
        rel++;
    }

    char rewritten[1024];
    SDL_strlcpy(rewritten, "assets://", sizeof(rewritten));
    SDL_strlcat(rewritten, rel, sizeof(rewritten));
    SDL_strlcpy(path, rewritten, path_len);
}
#endif

static void sdl_sound_build_path(const char* base_path, char* dst, size_t dst_len)
{
    if (!base_path || !base_path[0]) {
        dst[0] = '\0';
        return;
    }

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
        return;
    }

    const char* anchor = ANGBAND_DIR_XTRA;
    if (!anchor || !anchor[0]) {
        dst[0] = '\0';
        return;
    }

    path_build(dst, dst_len, anchor, base_path);

#ifdef __ANDROID__
    sdl_sound_to_asset_uri(dst, dst_len);
#endif
}

static bool sdl_sound_scan_folder(const char* folder_path, char files[][SDL_SOUND_NAME_LEN],
    int* file_count, int max_files)
{
    *file_count = 0;
    if (!folder_path || !folder_path[0]) {
        return false;
    }

    sound_scan_ctx ctx;
    ctx.files = files;
    ctx.file_count = file_count;
    ctx.max_files = max_files;

    if (!SDL_EnumerateDirectory(folder_path, sdl_sound_scan_enum_cb, &ctx)) {
        log_debug("Cannot open sound folder: %s", folder_path);
        return false;
    }

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
            continue;
        }

        char folder_path[1024];
        sdl_sound_build_path(config->events[i], folder_path, sizeof(folder_path));
        if (!folder_path[0]) {
            log_debug("Invalid path for sound event %d ('%s')", i,
                angband_sound_name[i] ? angband_sound_name[i] : "unknown");
            continue;
        }

        int count = 0;
        if (sdl_sound_scan_folder(folder_path, sound_state.bank.sound_files[i], &count,
                SDL_SOUND_MAX_VARIANTS)) {
            sound_state.bank.sound_counts[i] = count;
            loaded_events++;
            log_debug("Sound event '%s' (idx=%d): loaded %d file(s) from '%s'",
                angband_sound_name[i] ? angband_sound_name[i] : "unknown",
                i, count, folder_path);
        }
    }

    sound_state.bank_loaded = loaded_events > 0;
    log_info("Loaded sound events: %d/%d", loaded_events, MSG_MAX);
    return sound_state.bank_loaded;
}

static bool sdl_sound_has_any_events(const struct sound_config* config)
{
    if (!config) {
        return false;
    }

    for (int i = 0; i < MSG_MAX; i++) {
        if (config->events[i][0]) {
            return true;
        }
    }

    return false;
}

static bool sdl_sound_load_default_config(struct sound_config* defaults)
{
    if (!defaults || !ANGBAND_DIR_PREF || !ANGBAND_DIR_PREF[0]) {
        return false;
    }

    char defaults_path[1024];
    if (!path_build(defaults_path, sizeof(defaults_path), ANGBAND_DIR_PREF, "sound.json")) {
        return false;
    }

    if (streq(defaults_path, g_sound_config_path)) {
        return false;
    }

    sound_config_load(defaults_path, defaults);
    return sdl_sound_has_any_events(defaults);
}

static void sdl_sound_fill_missing_events_from_defaults(struct sound_config* config)
{
    if (!config) {
        return;
    }

    struct sound_config defaults;
    if (!sdl_sound_load_default_config(&defaults)) {
        log_warn("Could not load default sound event mappings from pref/sound.json");
        return;
    }

    int copied = 0;
    for (int i = 0; i < MSG_MAX; i++) {
        if (!config->events[i][0] && defaults.events[i][0]) {
            SDL_strlcpy(config->events[i], defaults.events[i], sizeof(config->events[i]));
            copied++;
        }
    }

    if (copied > 0) {
        log_info("Filled %d missing sound event mappings from defaults", copied);
        sound_config_save(g_sound_config_path, config);
    }
}

static SDL_AudioFormat sdl_sound_parse_format(const char* format_str)
{
    if (!format_str || !format_str[0]) {
        return SDL_AUDIO_S16;
    }

    char lowered[16];
    SDL_strlcpy(lowered, format_str, sizeof(lowered));
    for (char* p = lowered; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }

    if (streq(lowered, "s8")) return SDL_AUDIO_S8;
    if (streq(lowered, "u8")) return SDL_AUDIO_U8;
    if (streq(lowered, "s16")) return SDL_AUDIO_S16;
    if (streq(lowered, "s32")) return SDL_AUDIO_S32;
    if (streq(lowered, "f32")) return SDL_AUDIO_F32;
    return SDL_AUDIO_S16;
}

static bool sdl_sound_path_exists(const char* path)
{
    if (!path || !path[0]) {
        return false;
    }

    SDL_IOStream* probe = sdl_fopen(path, "rb");
    if (!probe) {
        return false;
    }

    sdl_fclose(probe);
    return true;
}

static bool sdl_sound_make_alternate_music_path(const char* path, char* alternate,
    size_t alternate_len)
{
    if (!path || !path[0] || !alternate || alternate_len == 0) {
        return false;
    }

    const char* ext = strrchr(path, '.');
    if (!ext) {
        return false;
    }

    size_t stem_len = (size_t)(ext - path);
    if (stem_len + 5 >= alternate_len) {
        return false;
    }

    memcpy(alternate, path, stem_len);
    alternate[stem_len] = '\0';

    if (SDL_strcasecmp(ext, ".wav") == 0) {
        SDL_strlcat(alternate, ".ogg", alternate_len);
        return true;
    }

    if (SDL_strcasecmp(ext, ".ogg") == 0 ||
        SDL_strcasecmp(ext, ".oga") == 0 ||
        SDL_strcasecmp(ext, ".opus") == 0) {
        SDL_strlcat(alternate, ".wav", alternate_len);
        return true;
    }

    return false;
}

static bool sdl_sound_resolve_music_path(const char* path, char* resolved,
    size_t resolved_len)
{
    if (!resolved || resolved_len == 0) {
        return false;
    }
    resolved[0] = '\0';

    if (!path || !path[0]) {
        return false;
    }

    if (sdl_sound_path_exists(path)) {
        SDL_strlcpy(resolved, path, resolved_len);
        return true;
    }

    char alternate[1024];
    if (sdl_sound_make_alternate_music_path(path, alternate, sizeof(alternate)) &&
        sdl_sound_path_exists(alternate)) {
        SDL_strlcpy(resolved, alternate, resolved_len);
        return true;
    }

    return false;
}

static bool sdl_sound_create_track_pool(void)
{
    sound_state.music_main_track = MIX_CreateTrack(sound_state.mixer);
    sound_state.music_menu_track = MIX_CreateTrack(sound_state.mixer);
    sound_state.music_ambient_track = MIX_CreateTrack(sound_state.mixer);
    if (!sound_state.music_main_track || !sound_state.music_menu_track ||
        !sound_state.music_ambient_track) {
        log_warn("Failed to create music tracks: %s", SDL_GetError());
        return false;
    }

    for (int i = 0; i < SDL_SOUND_MAX_ACTIVE_TRACKS; i++) {
        sound_state.sfx_tracks[i] = MIX_CreateTrack(sound_state.mixer);
        if (!sound_state.sfx_tracks[i]) {
            log_warn("Failed to create sound effect track %d: %s", i, SDL_GetError());
            return false;
        }
    }

    sound_state.next_sfx_track = 0;
    return true;
}

static bool sdl_sound_ensure_mixer(void)
{
    if (!g_sound_config.enabled) {
        return false;
    }

    if (sound_state.mixer) {
        if (sound_state.mixer_paused) {
            if (!MIX_ResumeAllTracks(sound_state.mixer)) {
                log_debug("Failed to resume paused tracks: %s", SDL_GetError());
            } else {
                sound_state.mixer_paused = false;
            }
        }
        return true;
    }

    if (!MIX_Init()) {
        log_warn("Failed to initialize SDL_mixer: %s", SDL_GetError());
        return false;
    }
    sound_state.mixer_initialized = true;

    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = g_sound_config.sample_rate;
    desired.format = sdl_sound_parse_format(g_sound_config.format);
    desired.channels = g_sound_config.channels;

    sound_state.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (!sound_state.mixer) {
        log_warn("Failed to create SDL_mixer device: %s", SDL_GetError());
        MIX_Quit();
        sound_state.mixer_initialized = false;
        return false;
    }

    if (!MIX_GetMixerFormat(sound_state.mixer, &sound_state.mixer_spec)) {
        log_warn("Failed to query SDL_mixer format, using requested spec");
        sound_state.mixer_spec = desired;
    }

    if (!sdl_sound_create_track_pool()) {
        sdl_sound_destroy_mixer();
        return false;
    }

    if (!MIX_SetMixerGain(sound_state.mixer, g_sound_config.volume_master)) {
        log_debug("Failed to set master mixer gain: %s", SDL_GetError());
    }

    int decoder_count = MIX_GetNumAudioDecoders();
    log_info("SDL_mixer opened audio device (freq=%d, channels=%d, format=0x%x, decoders=%d)",
        sound_state.mixer_spec.freq, sound_state.mixer_spec.channels,
        sound_state.mixer_spec.format, decoder_count);
    for (int i = 0; i < decoder_count; i++) {
        const char* decoder = MIX_GetAudioDecoder(i);
        log_debug("SDL_mixer decoder[%d]=%s", i, decoder ? decoder : "unknown");
    }

    sound_state.mixer_paused = false;
    return true;
}

static void sdl_sound_destroy_mixer(void)
{
    if (sound_state.music_main_track) {
        MIX_StopTrack(sound_state.music_main_track, 0);
    }
    if (sound_state.music_menu_track) {
        MIX_StopTrack(sound_state.music_menu_track, 0);
    }
    if (sound_state.music_ambient_track) {
        MIX_StopTrack(sound_state.music_ambient_track, 0);
    }
    for (int i = 0; i < SDL_SOUND_MAX_ACTIVE_TRACKS; i++) {
        if (sound_state.sfx_tracks[i]) {
            MIX_StopTrack(sound_state.sfx_tracks[i], 0);
        }
    }

    sound_state.music_main_current_path[0] = '\0';
    sdl_sound_destroy_cached_audio();

    if (sound_state.mixer) {
        MIX_DestroyMixer(sound_state.mixer);
        sound_state.mixer = NULL;
    }

    SDL_zero(sound_state.mixer_spec);
    sound_state.music_main_track = NULL;
    sound_state.music_menu_track = NULL;
    sound_state.music_ambient_track = NULL;
    for (int i = 0; i < SDL_SOUND_MAX_ACTIVE_TRACKS; i++) {
        sound_state.sfx_tracks[i] = NULL;
    }
    sound_state.next_sfx_track = 0;
    sound_state.mixer_paused = false;

    if (sound_state.mixer_initialized) {
        MIX_Quit();
        sound_state.mixer_initialized = false;
    }
}

static MIX_Audio* sdl_sound_get_sample_audio(int sound_idx, int sample_idx)
{
    if (sound_idx < 0 || sound_idx >= MSG_MAX ||
        sample_idx < 0 || sample_idx >= SDL_SOUND_MAX_VARIANTS) {
        return NULL;
    }

    if (sound_state.bank.sound_audio[sound_idx][sample_idx]) {
        return sound_state.bank.sound_audio[sound_idx][sample_idx];
    }

    if (!sdl_sound_ensure_mixer()) {
        return NULL;
    }

    const char* sample_path = sound_state.bank.sound_files[sound_idx][sample_idx];
    if (!sample_path[0]) {
        return NULL;
    }

    MIX_Audio* audio = MIX_LoadAudio(sound_state.mixer, sample_path, true);
    if (!audio) {
        log_warn("Failed to load sound sample '%s': %s", sample_path, SDL_GetError());
        return NULL;
    }

    sound_state.bank.sound_audio[sound_idx][sample_idx] = audio;
    return audio;
}

static bool sdl_sound_ensure_mutex(void)
{
    if (g_sound_mutex)
        return true;

    g_sound_mutex = SDL_CreateMutex();
    if (!g_sound_mutex) {
        log_warn("Could not create sound mutex: %s", SDL_GetError());
        return false;
    }
    return true;
}

static void sdl_sound_preload_event(int sound_idx)
{
    int sample_count;
    int loaded = 0;

    if (sound_idx < 0 || sound_idx >= MSG_MAX)
        return;

    sample_count = sound_state.bank.sound_counts[sound_idx];
    for (int sample_idx = 0; sample_idx < sample_count; sample_idx++) {
        if (sdl_sound_get_sample_audio(sound_idx, sample_idx))
            loaded++;
    }

    if (loaded > 0)
        log_debug("Preloaded %d/%d sample(s) for sound event %d", loaded,
            sample_count, sound_idx);
}

static MIX_Audio* sdl_sound_get_music_audio(const char* path)
{
    if (!path || !path[0]) {
        return NULL;
    }

    for (int i = 0; i < SDL_SOUND_MAX_MUSIC_CACHE; i++) {
        if (sound_state.music_cache[i].audio && streq(sound_state.music_cache[i].path, path)) {
            return sound_state.music_cache[i].audio;
        }
    }

    if (!sdl_sound_ensure_mixer()) {
        return NULL;
    }

    int free_slot = -1;
    for (int i = 0; i < SDL_SOUND_MAX_MUSIC_CACHE; i++) {
        if (!sound_state.music_cache[i].audio) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) {
        log_warn("Music cache exhausted while loading '%s'", path);
        return NULL;
    }

    MIX_Audio* audio = MIX_LoadAudio(sound_state.mixer, path, false);
    if (!audio) {
        log_warn("Failed to load music file '%s': %s", path, SDL_GetError());
        return NULL;
    }

    SDL_strlcpy(sound_state.music_cache[free_slot].path, path,
        sizeof(sound_state.music_cache[free_slot].path));
    sound_state.music_cache[free_slot].audio = audio;
    return audio;
}

static MIX_Track* sdl_sound_acquire_sfx_track(bool quiet)
{
    for (int n = 0; n < SDL_SOUND_MAX_ACTIVE_TRACKS; n++) {
        int idx = (sound_state.next_sfx_track + n) % SDL_SOUND_MAX_ACTIVE_TRACKS;
        MIX_Track* track = sound_state.sfx_tracks[idx];
        if (!track) {
            continue;
        }

        if (!MIX_TrackPlaying(track) && !MIX_TrackPaused(track)) {
            sound_state.next_sfx_track = (idx + 1) % SDL_SOUND_MAX_ACTIVE_TRACKS;
            return track;
        }
    }

    MIX_Track* track = sound_state.sfx_tracks[sound_state.next_sfx_track];
    if (track) {
        MIX_StopTrack(track, 0);
        if (!quiet) {
            log_warn(
                "Sound mixer saturated; recycling an active sound effect track");
        }
    }
    sound_state.next_sfx_track = (sound_state.next_sfx_track + 1) %
        SDL_SOUND_MAX_ACTIVE_TRACKS;
    return track;
}

static bool sdl_sound_play_track_audio(MIX_Track* track, MIX_Audio* audio, float gain,
    int loops, bool quiet)
{
    if (!track || !audio) {
        return false;
    }

    if (!MIX_SetTrackAudio(track, audio)) {
        if (!quiet)
            log_warn("Failed to assign audio to track: %s", SDL_GetError());
        return false;
    }

    if (!MIX_SetTrackGain(track, gain)) {
        if (!quiet)
            log_debug("Failed to set track gain: %s", SDL_GetError());
    }

    SDL_PropertiesID options = SDL_CreateProperties();
    if (!options) {
        if (!quiet) {
            log_warn("Failed to create SDL_mixer play options: %s",
                SDL_GetError());
        }
        return false;
    }

    bool success = SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, loops) &&
        SDL_SetBooleanProperty(options, MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN, true) &&
        MIX_PlayTrack(track, options);
    if (!success) {
        if (!quiet)
            log_warn("Failed to start track playback: %s", SDL_GetError());
    }

    SDL_DestroyProperties(options);
    return success;
}

static void sdl_music_stop_track(MIX_Track* track)
{
    if (track) {
        MIX_StopTrack(track, 0);
    }
}

static void sdl_music_stop_title_track(void)
{
    sdl_music_stop_track(sound_state.music_main_track);
    sound_state.music_main_current_path[0] = '\0';
}

static bool sdl_music_play_with_fallback(const char* primary_path,
    const char* fallback_path, MIX_Track* track, float gain, int loops,
    const char* label, char* resolved_path, size_t resolved_len)
{
    if (resolved_path && resolved_len) {
        resolved_path[0] = '\0';
    }

    const char* candidates[2] = { primary_path, fallback_path };
    for (int i = 0; i < 2; i++) {
        const char* candidate = candidates[i];
        if (!candidate || !candidate[0]) {
            continue;
        }
        if (i == 1 && primary_path && streq(primary_path, candidate)) {
            continue;
        }

        char resolved[1024];
        if (!sdl_sound_resolve_music_path(candidate, resolved, sizeof(resolved))) {
            continue;
        }

        MIX_Audio* audio = sdl_sound_get_music_audio(resolved);
        if (!audio) {
            continue;
        }

        if (sdl_sound_play_track_audio(track, audio, gain, loops, false)) {
            if (resolved_path && resolved_len) {
                SDL_strlcpy(resolved_path, resolved, resolved_len);
            }
            if (i == 1) {
                log_warn("Music '%s' unavailable at '%s'; falling back to '%s'",
                    label ? label : "track", primary_path ? primary_path : "", resolved);
            }
            return true;
        }
    }

    return false;
}

static bool sdl_music_play_title_track(const char* primary_path,
    const char* fallback_path, const char* label)
{
    char resolved_path[1024];
    char resolved_primary[1024];
    char resolved_fallback[1024];
    bool primary_available = false;
    bool fallback_available = false;

    if (!sound_state.music_main_enabled) {
        sdl_music_stop_main();
        sdl_music_stop_ambient();
        return false;
    }

    if (!sdl_sound_ensure_mixer()) {
        return false;
    }

    sdl_music_stop_ambient();
    sdl_music_stop_track(sound_state.music_menu_track);

    primary_available = sdl_sound_resolve_music_path(primary_path, resolved_primary,
        sizeof(resolved_primary));
    fallback_available = sdl_sound_resolve_music_path(fallback_path, resolved_fallback,
        sizeof(resolved_fallback));

    if ((MIX_TrackPlaying(sound_state.music_main_track) ||
            MIX_TrackPaused(sound_state.music_main_track)) &&
        sound_state.music_main_current_path[0]) {
        if (primary_available &&
            streq(resolved_primary, sound_state.music_main_current_path)) {
            return true;
        }

        /* Only treat the fallback as equivalent when the primary track is
         * unavailable; otherwise allow the upgrade to the preferred title cue. */
        if (!primary_available && fallback_available &&
            streq(resolved_fallback, sound_state.music_main_current_path)) {
            return true;
        }
    }

    if (!sdl_music_play_with_fallback(primary_path, fallback_path,
            sound_state.music_main_track, sound_state.music_main_volume, -1,
            label, resolved_path, sizeof(resolved_path))) {
        sound_state.music_main_current_path[0] = '\0';
        return false;
    }

    SDL_strlcpy(sound_state.music_main_current_path, resolved_path,
        sizeof(sound_state.music_main_current_path));
    return true;
}

bool sdl_sound_initialize(void)
{
    return true;
}

void sdl_sound_reload(void)
{
    if (!sdl_sound_ensure_mutex())
        return;

    SDL_LockMutex(g_sound_mutex);
    g_sound_generation++;
    sdl_sound_destroy_mixer();
    sdl_sound_reset_bank();

#ifdef SIL_USE_LOCAL_DATA
    if (ANGBAND_DIR_PREF && ANGBAND_DIR_PREF[0]) {
        path_build(g_sound_config_path, sizeof(g_sound_config_path), ANGBAND_DIR_PREF,
            "sound.json");
    } else {
        SDL_strlcpy(g_sound_config_path, "sound.json", sizeof(g_sound_config_path));
    }
#else
    if (ANGBAND_DIR_USER && ANGBAND_DIR_USER[0]) {
        path_build(g_sound_config_path, sizeof(g_sound_config_path), ANGBAND_DIR_USER,
            "sound.json");
    } else {
        SDL_strlcpy(g_sound_config_path, "sound.json", sizeof(g_sound_config_path));
    }
#endif

    sound_config_load(g_sound_config_path, &g_sound_config);
    sdl_sound_fill_missing_events_from_defaults(&g_sound_config);
    use_sound = g_sound_config.enabled;
    sound_state.bank_loaded = sdl_sound_load_from_config(&g_sound_config);

    if (g_sound_config.enabled && !sound_state.bank_loaded) {
        struct sound_config defaults;
        if (sdl_sound_load_default_config(&defaults)) {
            int replaced = 0;
            for (int i = 0; i < MSG_MAX; i++) {
                if (defaults.events[i][0] &&
                    !streq(g_sound_config.events[i], defaults.events[i])) {
                    SDL_strlcpy(g_sound_config.events[i], defaults.events[i],
                        sizeof(g_sound_config.events[i]));
                    replaced++;
                }
            }

            if (replaced > 0) {
                log_warn("Retrying sound event load with %d default event path(s)", replaced);
                sound_state.bank_loaded = sdl_sound_load_from_config(&g_sound_config);
                if (sound_state.bank_loaded) {
                    sound_config_save(g_sound_config_path, &g_sound_config);
                    log_info("Recovered sound effect mappings from defaults");
                }
            }
        }
    }

    if (g_sound_config.enabled && !sound_state.bank_loaded) {
        log_warn("Sound enabled but no effect samples were loaded (check sound paths and asset packaging)");
    }

    sound_state.enable_combat = g_sound_config.enable_combat;
    sound_state.enable_inventory = g_sound_config.enable_inventory;
    sound_state.enable_walk = g_sound_config.enable_walk;
    sound_state.enable_doors = g_sound_config.enable_doors;
    sound_state.enable_monster_hits = g_sound_config.enable_monster_hits;
    sound_state.enable_traps = g_sound_config.enable_traps;
    sound_state.volume_combat = g_sound_config.volume_combat;
    sound_state.volume_inventory = g_sound_config.volume_inventory;
    sound_state.volume_walk = g_sound_config.volume_walk;
    sound_state.volume_doors = g_sound_config.volume_doors;
    sound_state.volume_monster_hits = g_sound_config.volume_monster_hits;
    sound_state.volume_traps = g_sound_config.volume_traps;
    sound_state.volume_other = g_sound_config.volume_other;
    sound_state.music_main_enabled = g_sound_config.music_main_enabled;
    sound_state.music_ambient_enabled = g_sound_config.music_ambient_enabled;
    sound_state.music_main_volume = g_sound_config.music_main_volume;
    sound_state.music_ambient_volume = g_sound_config.music_ambient_volume;
    sdl_sound_build_path(g_sound_config.music_main_path, sound_state.music_main_path,
        sizeof(sound_state.music_main_path));
    sdl_sound_build_path(g_sound_config.music_main_full_path,
        sound_state.music_main_full_path, sizeof(sound_state.music_main_full_path));
    sdl_sound_build_path(g_sound_config.music_ambient_path,
        sound_state.music_ambient_path, sizeof(sound_state.music_ambient_path));
    sdl_sound_build_path(g_sound_config.music_death_path, sound_state.music_death_path,
        sizeof(sound_state.music_death_path));

    log_debug("Sound config: enabled=%d, bank_loaded=%d", g_sound_config.enabled,
        sound_state.bank_loaded);
    log_debug("Music paths: main='%s', main_full='%s', ambient='%s', death='%s'",
        sound_state.music_main_path, sound_state.music_main_full_path,
        sound_state.music_ambient_path, sound_state.music_death_path);

    if (g_sound_config.enabled && !sdl_sound_ensure_mixer()) {
        log_warn("Sound enabled but SDL_mixer could not initialize");
        g_sound_loaded_once = true;
        SDL_UnlockMutex(g_sound_mutex);
        return;
    }

    /* Result cues are timer-driven; avoid a first-use decode at cue time. */
    if (g_sound_config.enabled && sound_state.bank_loaded) {
        sdl_sound_preload_event(MSG_HIT);
        sdl_sound_preload_event(MSG_ARMOR);
    }

    sdl_music_update_volumes();
    g_sound_loaded_once = true;
    SDL_UnlockMutex(g_sound_mutex);
}

void sdl_sound_shutdown(void)
{
    if (g_sound_mutex)
        SDL_LockMutex(g_sound_mutex);
    g_sound_generation++;
    sdl_sound_destroy_mixer();
    sdl_sound_reset_bank();
    g_sound_loaded_once = false;
    if (g_sound_mutex)
        SDL_UnlockMutex(g_sound_mutex);
}

void sdl_music_play_main(void)
{
    log_debug("Starting title music: %s", sound_state.music_main_path);
    (void)sdl_music_play_title_track(sound_state.music_main_path, NULL, "main");
}

void sdl_music_play_main_full(void)
{
    log_debug("Starting full title music: %s", sound_state.music_main_full_path);
    (void)sdl_music_play_title_track(sound_state.music_main_full_path,
        sound_state.music_main_path, "main_full");
}

void sdl_music_play_death(void)
{
    log_debug("Starting death music: %s", sound_state.music_death_path);
    (void)sdl_music_play_title_track(sound_state.music_death_path, NULL, "death");
}

void sdl_music_play_menu_theme(void)
{
    if (!sound_state.music_main_enabled) {
        sdl_music_stop_main();
        return;
    }

    if (!sdl_sound_ensure_mixer()) {
        return;
    }

    sdl_music_stop_title_track();
    sdl_music_play_ambient();
    sdl_music_stop_track(sound_state.music_menu_track);

    log_debug("Starting menu theme overlay: %s", sound_state.music_main_full_path);
    (void)sdl_music_play_with_fallback(sound_state.music_main_full_path,
        sound_state.music_main_path, sound_state.music_menu_track,
        sound_state.music_main_volume, 0, "main_full", NULL, 0);
}

void sdl_music_play_ambient(void)
{
    if (!sound_state.music_ambient_enabled) {
        sdl_music_stop_ambient();
        return;
    }

    if (!sdl_sound_ensure_mixer()) {
        return;
    }

    if (MIX_TrackPlaying(sound_state.music_ambient_track) ||
        MIX_TrackPaused(sound_state.music_ambient_track)) {
        return;
    }

    log_debug("Starting persistent ambient music: %s", sound_state.music_ambient_path);
    (void)sdl_music_play_with_fallback(sound_state.music_ambient_path, NULL,
        sound_state.music_ambient_track, sound_state.music_ambient_volume, -1,
        "ambient", NULL, 0);
}

void sdl_music_stop_main(void)
{
    sdl_music_stop_title_track();
    sdl_music_stop_track(sound_state.music_menu_track);
}

void sdl_music_stop_ambient(void)
{
    sdl_music_stop_track(sound_state.music_ambient_track);
}

void sdl_music_update(void)
{
}

void sdl_music_update_volumes(void)
{
    if (!sound_state.mixer) {
        return;
    }

    if (!MIX_SetMixerGain(sound_state.mixer, g_sound_config.volume_master)) {
        log_debug("Failed to update mixer master gain: %s", SDL_GetError());
    }

    if (sound_state.music_main_track &&
        !MIX_SetTrackGain(sound_state.music_main_track, sound_state.music_main_volume)) {
        log_debug("Failed to update main music volume: %s", SDL_GetError());
    }

    if (sound_state.music_menu_track &&
        !MIX_SetTrackGain(sound_state.music_menu_track, sound_state.music_main_volume)) {
        log_debug("Failed to update menu theme volume: %s", SDL_GetError());
    }

    if (sound_state.music_ambient_track &&
        !MIX_SetTrackGain(sound_state.music_ambient_track,
            sound_state.music_ambient_volume)) {
        log_debug("Failed to update ambient music volume: %s", SDL_GetError());
    }
}

void sdl_music_request_welcome_main_once(void)
{
    g_music_force_main_on_next_welcome = true;
}

bool sdl_music_consume_welcome_main_once(void)
{
    bool consume = g_music_force_main_on_next_welcome;
    g_music_force_main_on_next_welcome = false;
    return consume;
}

static bool sdl_sound_play_sample_locked(int sound_idx, int sample_idx,
    bool quiet)
{
    if (sound_idx < 0 || sound_idx >= MSG_MAX || !g_sound_config.enabled) {
        return false;
    }

    if (!is_sound_enabled(sound_idx)) {
        return false;
    }

    int sample_count = sound_state.bank.sound_counts[sound_idx];
    if (sample_count <= 0 || !sdl_sound_ensure_mixer()) {
        return false;
    }

    if (sample_idx < 0 || sample_idx >= sample_count)
        return false;

    const char* sample_path = sound_state.bank.sound_files[sound_idx][sample_idx];
    if (!sample_path[0]) {
        return false;
    }

    MIX_Audio* audio = sdl_sound_get_sample_audio(sound_idx, sample_idx);
    MIX_Track* track = sdl_sound_acquire_sfx_track(quiet);
    if (!audio || !track) {
        return false;
    }

    if (!quiet)
        log_trace("sdl_sound_handle: idx=%d path='%s'", sound_idx, sample_path);
    return sdl_sound_play_track_audio(track, audio, get_sound_volume(sound_idx),
        0, quiet);
}

void sdl_sound_handle(int sound_idx)
{
    int sample_count;
    int sample_idx;

    if (sound_idx < 0 || sound_idx >= MSG_MAX || !g_sound_config.enabled)
        return;
    if (!sdl_sound_ensure_mutex())
        return;

    SDL_LockMutex(g_sound_mutex);
    sample_count = sound_state.bank.sound_counts[sound_idx];
    if (sample_count > 0) {
        sample_idx = (sample_count > 1) ? Rand_div(sample_count) : 0;
        (void)sdl_sound_play_sample_locked(sound_idx, sample_idx, false);
    }
    SDL_UnlockMutex(g_sound_mutex);
}

static Uint32 SDLCALL sdl_sound_deferred_timer_cb(void* userdata,
    SDL_TimerID id, Uint32 interval)
{
    delayed_sound_request* request = (delayed_sound_request*)userdata;
    Uint64 now_ns;
    Uint64 late_ms = 0;
    bool played = false;

    (void)id;
    (void)interval;

    if (!request)
        return 0;

    if (g_sound_mutex) {
        SDL_LockMutex(g_sound_mutex);
        if (request->generation == g_sound_generation) {
            played = sdl_sound_play_sample_locked(request->sound_idx,
                request->sample_idx, true);
        }
        SDL_UnlockMutex(g_sound_mutex);
    }

    now_ns = SDL_GetTicksNS();
    if (now_ns > request->due_ns)
        late_ms = (now_ns - request->due_ns) / 1000000ULL;

    /*
     * The project logger is main-thread oriented. Playback is already active;
     * send only the measured result back to the event loop for diagnostics.
     */
    if (g_sound_diagnostic_event != (Uint32)-1) {
        SDL_Event ev;

        SDL_zero(ev);
        ev.type = g_sound_diagnostic_event;
        ev.user.code = request->sound_idx;
        ev.user.data1 = (void*)(uintptr_t)late_ms;
        ev.user.data2 = (void*)(uintptr_t)(played ? 1 : 0);
        SDL_PushEvent(&ev);
    }

    SDL_free(request);
    return 0;
}

void sdl_sound_handle_delayed(int sound_idx, Uint32 delay_ms)
{
    delayed_sound_request* request;
    int sample_count;

    if (sound_idx < 0 || sound_idx >= MSG_MAX) {
        return;
    }

    if (delay_ms == 0) {
        sdl_sound_handle(sound_idx);
        return;
    }

    if (!sdl_sound_ensure_mutex()) {
        sdl_sound_handle(sound_idx);
        return;
    }

    if (g_sound_diagnostic_event == (Uint32)-1)
        g_sound_diagnostic_event = SDL_RegisterEvents(1);

    request = SDL_calloc(1, sizeof(*request));
    if (!request) {
        log_warn("Could not allocate delayed sound request");
        sdl_sound_handle(sound_idx);
        return;
    }

    SDL_LockMutex(g_sound_mutex);
    sample_count = sound_state.bank.sound_counts[sound_idx];
    if (!g_sound_config.enabled || !is_sound_enabled(sound_idx)
        || sample_count <= 0 || !sdl_sound_ensure_mixer())
    {
        SDL_UnlockMutex(g_sound_mutex);
        SDL_free(request);
        return;
    }

    request->sound_idx = sound_idx;
    request->sample_idx = (sample_count > 1) ? Rand_div(sample_count) : 0;
    request->generation = g_sound_generation;
    request->due_ns = SDL_GetTicksNS() + (Uint64)delay_ms * 1000000ULL;

    /* Decode on the scheduling thread; the timer callback only starts mixing. */
    if (!sdl_sound_get_sample_audio(sound_idx, request->sample_idx)) {
        SDL_UnlockMutex(g_sound_mutex);
        SDL_free(request);
        return;
    }
    SDL_UnlockMutex(g_sound_mutex);

    if (!SDL_AddTimer(delay_ms, sdl_sound_deferred_timer_cb, request)) {
        log_warn("SDL_AddTimer failed: %s", SDL_GetError());
        SDL_free(request);
        sdl_sound_handle(sound_idx);
    }
}

bool sdl_sound_try_handle_event(const SDL_Event* ev)
{
    Uint64 late_ms;
    bool played;

    if (!ev || g_sound_diagnostic_event == (Uint32)-1
        || ev->type != g_sound_diagnostic_event)
    {
        return false;
    }

    late_ms = (Uint64)(uintptr_t)ev->user.data1;
    played = ((uintptr_t)ev->user.data2 != 0);
    if (late_ms >= 20) {
        log_warn("[SLOWAUDIO] delayed cue %d started %llu ms late (played=%d)",
            ev->user.code, (unsigned long long)late_ms, played ? 1 : 0);
    } else {
        log_trace("Delayed cue %d started %llu ms after deadline (played=%d)",
            ev->user.code, (unsigned long long)late_ms, played ? 1 : 0);
    }
    return true;
}

void sdl_init_sounds(void)
{
    if (!g_sound_loaded_once)
        sdl_sound_reload();
}

struct sound_config* sdl_sound_get_config(void)
{
    return &g_sound_config;
}

void sdl_sound_save_config(void)
{
    sound_state.enable_combat = g_sound_config.enable_combat;
    sound_state.enable_inventory = g_sound_config.enable_inventory;
    sound_state.enable_walk = g_sound_config.enable_walk;
    sound_state.enable_doors = g_sound_config.enable_doors;
    sound_state.enable_monster_hits = g_sound_config.enable_monster_hits;
    sound_state.enable_traps = g_sound_config.enable_traps;
    sound_state.volume_combat = g_sound_config.volume_combat;
    sound_state.volume_inventory = g_sound_config.volume_inventory;
    sound_state.volume_walk = g_sound_config.volume_walk;
    sound_state.volume_doors = g_sound_config.volume_doors;
    sound_state.volume_monster_hits = g_sound_config.volume_monster_hits;
    sound_state.volume_traps = g_sound_config.volume_traps;
    sound_state.volume_other = g_sound_config.volume_other;
    sound_state.music_main_enabled = g_sound_config.music_main_enabled;
    sound_state.music_ambient_enabled = g_sound_config.music_ambient_enabled;
    sound_state.music_main_volume = g_sound_config.music_main_volume;
    sound_state.music_ambient_volume = g_sound_config.music_ambient_volume;
    sdl_sound_build_path(g_sound_config.music_main_path, sound_state.music_main_path,
        sizeof(sound_state.music_main_path));
    sdl_sound_build_path(g_sound_config.music_main_full_path,
        sound_state.music_main_full_path, sizeof(sound_state.music_main_full_path));
    sdl_sound_build_path(g_sound_config.music_ambient_path,
        sound_state.music_ambient_path, sizeof(sound_state.music_ambient_path));
    sdl_sound_build_path(g_sound_config.music_death_path, sound_state.music_death_path,
        sizeof(sound_state.music_death_path));
    use_sound = g_sound_config.enabled;

    if (!g_sound_config.enabled) {
        if (sound_state.mixer && !MIX_PauseAllTracks(sound_state.mixer)) {
            log_debug("Failed to pause mixer tracks: %s", SDL_GetError());
        } else {
            sound_state.mixer_paused = sound_state.mixer != NULL;
        }
    } else if (sdl_sound_ensure_mixer()) {
        sound_state.mixer_paused = false;
    }

    if (!sound_state.music_main_enabled) {
        sdl_music_stop_main();
    }
    if (!sound_state.music_ambient_enabled) {
        sdl_music_stop_ambient();
    }

    sdl_music_update_volumes();
    sound_config_save(g_sound_config_path, &g_sound_config);
    log_debug("Sound configuration saved to %s", g_sound_config_path);
}
