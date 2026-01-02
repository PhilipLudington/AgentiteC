#include "agentite/agentite.h"
#include "agentite/audio.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Sound data (fully loaded in memory)
struct Agentite_Sound {
    Uint8 *data;
    Uint32 length;
    SDL_AudioSpec spec;
};

// Music data (streamed from file)
struct Agentite_Music {
    char *filepath;
    SDL_AudioSpec spec;
    Uint8 *data;
    Uint32 length;
    bool loaded;
};

// Audio channel for mixing
typedef struct {
    Agentite_Sound *sound;
    Uint32 position;
    float volume;
    float pan;
    bool loop;
    bool active;
} AudioChannel;

// Main audio system
struct Agentite_Audio {
    SDL_AudioStream *stream;
    SDL_AudioDeviceID device_id;
    SDL_AudioSpec device_spec;

    // Mixing channels for sounds
    AudioChannel channels[AGENTITE_AUDIO_MAX_CHANNELS];
    int next_handle;

    // Music state
    Agentite_Music *current_music;
    Uint32 music_position;
    float music_volume;
    bool music_loop;
    bool music_playing;
    bool music_paused;

    // Volume controls
    float master_volume;
    float sound_volume;
    float global_music_volume;

    // Mixing buffer
    float *mix_buffer;
    int mix_buffer_size;
};

// Helper: clamp float to 0-1
static float clampf(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

// Audio callback - mix all active sounds and music
static void audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    (void)total_amount;
    Agentite_Audio *audio = (Agentite_Audio *)userdata;

    if (additional_amount <= 0) return;

    int samples_needed = additional_amount / sizeof(float);

    // Ensure mix buffer is large enough
    if (samples_needed > audio->mix_buffer_size) {
        audio->mix_buffer = (float*)realloc(audio->mix_buffer, samples_needed * sizeof(float));
        audio->mix_buffer_size = samples_needed;
    }

    // Clear mix buffer
    memset(audio->mix_buffer, 0, samples_needed * sizeof(float));

    // Mix all active sound channels
    for (int ch = 0; ch < AGENTITE_AUDIO_MAX_CHANNELS; ch++) {
        AudioChannel *channel = &audio->channels[ch];
        if (!channel->active || !channel->sound) continue;

        Agentite_Sound *sound = channel->sound;
        float *src = (float *)sound->data;
        int src_samples = sound->length / sizeof(float);

        float vol_l = channel->volume * audio->sound_volume * audio->master_volume;
        float vol_r = vol_l;

        // Apply pan (-1 = left, 0 = center, +1 = right)
        if (channel->pan < 0) {
            vol_r *= (1.0f + channel->pan);
        } else if (channel->pan > 0) {
            vol_l *= (1.0f - channel->pan);
        }

        int samples_written = 0;
        while (samples_written < samples_needed) {
            Uint32 src_pos = channel->position;

            if (src_pos >= (Uint32)src_samples) {
                if (channel->loop) {
                    channel->position = 0;
                    src_pos = 0;
                } else {
                    channel->active = false;
                    break;
                }
            }

            // Mix stereo samples
            int remaining_src = src_samples - src_pos;
            int remaining_dst = samples_needed - samples_written;
            int to_mix = (remaining_src < remaining_dst) ? remaining_src : remaining_dst;

            // Ensure we mix stereo pairs
            to_mix = (to_mix / 2) * 2;

            for (int i = 0; i < to_mix; i += 2) {
                audio->mix_buffer[samples_written + i] += src[src_pos + i] * vol_l;
                audio->mix_buffer[samples_written + i + 1] += src[src_pos + i + 1] * vol_r;
            }

            channel->position += to_mix;
            samples_written += to_mix;
        }
    }

    // Mix music
    if (audio->current_music && audio->music_playing && !audio->music_paused) {
        Agentite_Music *music = audio->current_music;
        if (music->loaded && music->data) {
            float *src = (float *)music->data;
            int src_samples = music->length / sizeof(float);
            float vol = audio->music_volume * audio->global_music_volume * audio->master_volume;

            int samples_written = 0;
            while (samples_written < samples_needed) {
                Uint32 src_pos = audio->music_position;

                if (src_pos >= (Uint32)src_samples) {
                    if (audio->music_loop) {
                        audio->music_position = 0;
                        src_pos = 0;
                    } else {
                        audio->music_playing = false;
                        break;
                    }
                }

                int remaining_src = src_samples - src_pos;
                int remaining_dst = samples_needed - samples_written;
                int to_mix = (remaining_src < remaining_dst) ? remaining_src : remaining_dst;
                to_mix = (to_mix / 2) * 2;

                for (int i = 0; i < to_mix; i++) {
                    audio->mix_buffer[samples_written + i] += src[src_pos + i] * vol;
                }

                audio->music_position += to_mix;
                samples_written += to_mix;
            }
        }
    }

    // Clamp final output
    for (int i = 0; i < samples_needed; i++) {
        audio->mix_buffer[i] = clampf(audio->mix_buffer[i], -1.0f, 1.0f);
    }

    // Write to stream
    SDL_PutAudioStreamData(stream, audio->mix_buffer, samples_needed * sizeof(float));
}

Agentite_Audio *agentite_audio_init(void) {
    Agentite_Audio *audio = AGENTITE_ALLOC(Agentite_Audio);
    if (!audio) return NULL;

    // Initialize default volumes
    audio->master_volume = 1.0f;
    audio->sound_volume = 1.0f;
    audio->global_music_volume = 1.0f;
    audio->music_volume = 1.0f;

    // Set up desired audio spec (float32, stereo, 48kHz)
    SDL_AudioSpec desired_spec = {
        .format = SDL_AUDIO_F32,
        .channels = 2,
        .freq = 48000
    };

    // Create audio stream with callback
    audio->stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &desired_spec,
        audio_callback,
        audio
    );

    if (!audio->stream) {
        agentite_set_error_from_sdl("Failed to create audio stream");
        free(audio);
        return NULL;
    }

    // Get actual device spec
    int sample_frames;
    SDL_GetAudioStreamFormat(audio->stream, NULL, &audio->device_spec);
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio->stream), &audio->device_spec, &sample_frames);

    // Allocate initial mix buffer
    audio->mix_buffer_size = 4096;
    audio->mix_buffer = (float*)malloc(audio->mix_buffer_size * sizeof(float));

    // Start playback
    SDL_ResumeAudioStreamDevice(audio->stream);

    SDL_Log("Carbon Audio initialized: %dHz, %d channels, format=%d",
            audio->device_spec.freq, audio->device_spec.channels, audio->device_spec.format);

    return audio;
}

void agentite_audio_shutdown(Agentite_Audio *audio) {
    if (!audio) return;

    // Stop all sounds
    agentite_sound_stop_all(audio);
    agentite_music_stop(audio);

    // Destroy audio stream
    if (audio->stream) {
        SDL_DestroyAudioStream(audio->stream);
    }

    // Free mix buffer
    free(audio->mix_buffer);

    free(audio);
    SDL_Log("Carbon Audio shutdown complete");
}

// Helper: convert audio to float32 stereo at device sample rate
static bool convert_audio_to_device(Agentite_Audio *audio, Uint8 *src_data, Uint32 src_length,
                                    SDL_AudioSpec *src_spec, Uint8 **out_data, Uint32 *out_length) {
    SDL_AudioSpec dst_spec = {
        .format = SDL_AUDIO_F32,
        .channels = 2,
        .freq = audio->device_spec.freq
    };

    // Create a temporary stream for conversion
    SDL_AudioStream *conv = SDL_CreateAudioStream(src_spec, &dst_spec);
    if (!conv) {
        agentite_set_error_from_sdl("Failed to create conversion stream");
        return false;
    }

    // Put source data
    if (!SDL_PutAudioStreamData(conv, src_data, src_length)) {
        agentite_set_error_from_sdl("Failed to put data in conversion stream");
        SDL_DestroyAudioStream(conv);
        return false;
    }

    // Flush to signal end of input
    SDL_FlushAudioStream(conv);

    // Get converted data
    int available = SDL_GetAudioStreamAvailable(conv);
    if (available <= 0) {
        SDL_DestroyAudioStream(conv);
        return false;
    }

    *out_data = (uint8_t*)malloc(available);
    *out_length = SDL_GetAudioStreamData(conv, *out_data, available);

    SDL_DestroyAudioStream(conv);
    return (*out_length > 0);
}

Agentite_Sound *agentite_sound_load(Agentite_Audio *audio, const char *filepath) {
    if (!audio || !filepath) return NULL;

    // Load WAV file
    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV(filepath, &spec, &wav_data, &wav_length)) {
        agentite_set_error("Failed to load WAV '%s': %s", filepath, SDL_GetError());
        return NULL;
    }

    Agentite_Sound *sound = AGENTITE_ALLOC(Agentite_Sound);
    if (!sound) {
        SDL_free(wav_data);
        return NULL;
    }

    // Convert to device format
    if (!convert_audio_to_device(audio, wav_data, wav_length, &spec,
                                 &sound->data, &sound->length)) {
        agentite_set_error("Failed to convert audio format for '%s'", filepath);
        SDL_free(wav_data);
        free(sound);
        return NULL;
    }

    SDL_free(wav_data);

    sound->spec.format = SDL_AUDIO_F32;
    sound->spec.channels = 2;
    sound->spec.freq = audio->device_spec.freq;

    SDL_Log("Loaded sound '%s': %u bytes", filepath, sound->length);
    return sound;
}

Agentite_Sound *agentite_sound_load_wav_memory(Agentite_Audio *audio, const void *data, size_t size) {
    if (!audio || !data || size == 0) return NULL;

    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (!io) return NULL;

    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV_IO(io, true, &spec, &wav_data, &wav_length)) {
        agentite_set_error_from_sdl("Failed to load WAV from memory");
        return NULL;
    }

    Agentite_Sound *sound = AGENTITE_ALLOC(Agentite_Sound);
    if (!sound) {
        SDL_free(wav_data);
        return NULL;
    }

    if (!convert_audio_to_device(audio, wav_data, wav_length, &spec,
                                 &sound->data, &sound->length)) {
        SDL_free(wav_data);
        free(sound);
        return NULL;
    }

    SDL_free(wav_data);

    sound->spec.format = SDL_AUDIO_F32;
    sound->spec.channels = 2;
    sound->spec.freq = audio->device_spec.freq;

    return sound;
}

void agentite_sound_destroy(Agentite_Audio *audio, Agentite_Sound *sound) {
    if (!audio || !sound) return;

    // Stop any channels using this sound
    for (int i = 0; i < AGENTITE_AUDIO_MAX_CHANNELS; i++) {
        if (audio->channels[i].sound == sound) {
            audio->channels[i].active = false;
            audio->channels[i].sound = NULL;
        }
    }

    free(sound->data);
    free(sound);
}

Agentite_Music *agentite_music_load(Agentite_Audio *audio, const char *filepath) {
    if (!audio || !filepath) return NULL;

    // Load WAV file for music (same as sound for now)
    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV(filepath, &spec, &wav_data, &wav_length)) {
        agentite_set_error("Failed to load music '%s': %s", filepath, SDL_GetError());
        return NULL;
    }

    Agentite_Music *music = AGENTITE_ALLOC(Agentite_Music);
    if (!music) {
        SDL_free(wav_data);
        return NULL;
    }

    music->filepath = strdup(filepath);

    // Convert to device format
    if (!convert_audio_to_device(audio, wav_data, wav_length, &spec,
                                 &music->data, &music->length)) {
        agentite_set_error("Failed to convert music format for '%s'", filepath);
        SDL_free(wav_data);
        free(music->filepath);
        free(music);
        return NULL;
    }

    SDL_free(wav_data);

    music->spec.format = SDL_AUDIO_F32;
    music->spec.channels = 2;
    music->spec.freq = audio->device_spec.freq;
    music->loaded = true;

    SDL_Log("Loaded music '%s': %u bytes", filepath, music->length);
    return music;
}

void agentite_music_destroy(Agentite_Audio *audio, Agentite_Music *music) {
    if (!audio || !music) return;

    // Stop if currently playing
    if (audio->current_music == music) {
        agentite_music_stop(audio);
    }

    free(music->filepath);
    free(music->data);
    free(music);
}

// Find a free channel
static int find_free_channel(Agentite_Audio *audio) {
    for (int i = 0; i < AGENTITE_AUDIO_MAX_CHANNELS; i++) {
        if (!audio->channels[i].active) {
            return i;
        }
    }
    // All channels busy - find oldest one and steal it
    return 0;
}

Agentite_SoundHandle agentite_sound_play(Agentite_Audio *audio, Agentite_Sound *sound) {
    return agentite_sound_play_ex(audio, sound, 1.0f, 0.0f, false);
}

Agentite_SoundHandle agentite_sound_play_ex(Agentite_Audio *audio, Agentite_Sound *sound,
                                         float volume, float pan, bool loop) {
    if (!audio || !sound) return AGENTITE_INVALID_SOUND_HANDLE;

    int ch = find_free_channel(audio);

    audio->channels[ch].sound = sound;
    audio->channels[ch].position = 0;
    audio->channels[ch].volume = clampf(volume, 0.0f, 1.0f);
    audio->channels[ch].pan = clampf(pan, -1.0f, 1.0f);
    audio->channels[ch].loop = loop;
    audio->channels[ch].active = true;

    // Generate unique handle (channel index + generation counter)
    int handle = ch + (audio->next_handle++ << 8);
    return handle;
}

static int handle_to_channel(Agentite_SoundHandle handle) {
    if (handle == AGENTITE_INVALID_SOUND_HANDLE) return -1;
    int ch = handle & 0xFF;
    if (ch < 0 || ch >= AGENTITE_AUDIO_MAX_CHANNELS) return -1;
    return ch;
}

void agentite_sound_stop(Agentite_Audio *audio, Agentite_SoundHandle handle) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0) {
        audio->channels[ch].active = false;
    }
}

void agentite_sound_set_volume(Agentite_Audio *audio, Agentite_SoundHandle handle, float volume) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0 && audio->channels[ch].active) {
        audio->channels[ch].volume = clampf(volume, 0.0f, 1.0f);
    }
}

void agentite_sound_set_pan(Agentite_Audio *audio, Agentite_SoundHandle handle, float pan) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0 && audio->channels[ch].active) {
        audio->channels[ch].pan = clampf(pan, -1.0f, 1.0f);
    }
}

void agentite_sound_set_loop(Agentite_Audio *audio, Agentite_SoundHandle handle, bool loop) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0 && audio->channels[ch].active) {
        audio->channels[ch].loop = loop;
    }
}

bool agentite_sound_is_playing(Agentite_Audio *audio, Agentite_SoundHandle handle) {
    if (!audio) return false;
    int ch = handle_to_channel(handle);
    if (ch >= 0) {
        return audio->channels[ch].active;
    }
    return false;
}

void agentite_sound_stop_all(Agentite_Audio *audio) {
    if (!audio) return;
    for (int i = 0; i < AGENTITE_AUDIO_MAX_CHANNELS; i++) {
        audio->channels[i].active = false;
    }
}

void agentite_music_play(Agentite_Audio *audio, Agentite_Music *music) {
    agentite_music_play_ex(audio, music, 1.0f, true);
}

void agentite_music_play_ex(Agentite_Audio *audio, Agentite_Music *music, float volume, bool loop) {
    if (!audio || !music) return;

    audio->current_music = music;
    audio->music_position = 0;
    audio->music_volume = clampf(volume, 0.0f, 1.0f);
    audio->music_loop = loop;
    audio->music_playing = true;
    audio->music_paused = false;
}

void agentite_music_stop(Agentite_Audio *audio) {
    if (!audio) return;
    audio->music_playing = false;
    audio->music_paused = false;
    audio->current_music = NULL;
    audio->music_position = 0;
}

void agentite_music_pause(Agentite_Audio *audio) {
    if (!audio) return;
    audio->music_paused = true;
}

void agentite_music_resume(Agentite_Audio *audio) {
    if (!audio) return;
    audio->music_paused = false;
}

void agentite_music_set_volume(Agentite_Audio *audio, float volume) {
    if (!audio) return;
    audio->music_volume = clampf(volume, 0.0f, 1.0f);
}

bool agentite_music_is_playing(Agentite_Audio *audio) {
    return audio && audio->music_playing && !audio->music_paused;
}

bool agentite_music_is_paused(Agentite_Audio *audio) {
    return audio && audio->music_playing && audio->music_paused;
}

void agentite_audio_set_master_volume(Agentite_Audio *audio, float volume) {
    if (!audio) return;
    audio->master_volume = clampf(volume, 0.0f, 1.0f);
}

float agentite_audio_get_master_volume(Agentite_Audio *audio) {
    return audio ? audio->master_volume : 0.0f;
}

void agentite_audio_set_sound_volume(Agentite_Audio *audio, float volume) {
    if (!audio) return;
    audio->sound_volume = clampf(volume, 0.0f, 1.0f);
}

float agentite_audio_get_sound_volume(Agentite_Audio *audio) {
    return audio ? audio->sound_volume : 0.0f;
}

void agentite_audio_set_music_volume(Agentite_Audio *audio, float volume) {
    if (!audio) return;
    audio->global_music_volume = clampf(volume, 0.0f, 1.0f);
}

float agentite_audio_get_music_volume(Agentite_Audio *audio) {
    return audio ? audio->global_music_volume : 0.0f;
}

void agentite_audio_update(Agentite_Audio *audio) {
    // Currently no-op - mixing is handled in callback
    // Future: could be used for streaming music from disk
    (void)audio;
}

/* ============================================================================
 * Asset Handle Integration
 * ============================================================================ */

#include "agentite/asset.h"

Agentite_AssetHandle agentite_sound_load_asset(Agentite_Audio *audio,
                                                Agentite_AssetRegistry *registry,
                                                const char *path)
{
    if (!audio || !registry || !path) {
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    /* Check if already loaded */
    Agentite_AssetHandle existing = agentite_asset_lookup(registry, path);
    if (agentite_asset_is_valid(existing)) {
        /* Already loaded - add reference and return */
        agentite_asset_addref(registry, existing);
        return existing;
    }

    /* Load the sound */
    Agentite_Sound *sound = agentite_sound_load(audio, path);
    if (!sound) {
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    /* Register with asset system */
    Agentite_AssetHandle handle = agentite_asset_register(
        registry, path, AGENTITE_ASSET_SOUND, sound);

    if (!agentite_asset_is_valid(handle)) {
        /* Registration failed - clean up sound */
        agentite_sound_destroy(audio, sound);
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    return handle;
}

Agentite_AssetHandle agentite_music_load_asset(Agentite_Audio *audio,
                                                Agentite_AssetRegistry *registry,
                                                const char *path)
{
    if (!audio || !registry || !path) {
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    /* Check if already loaded */
    Agentite_AssetHandle existing = agentite_asset_lookup(registry, path);
    if (agentite_asset_is_valid(existing)) {
        /* Already loaded - add reference and return */
        agentite_asset_addref(registry, existing);
        return existing;
    }

    /* Load the music */
    Agentite_Music *music = agentite_music_load(audio, path);
    if (!music) {
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    /* Register with asset system */
    Agentite_AssetHandle handle = agentite_asset_register(
        registry, path, AGENTITE_ASSET_MUSIC, music);

    if (!agentite_asset_is_valid(handle)) {
        /* Registration failed - clean up music */
        agentite_music_destroy(audio, music);
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    return handle;
}

Agentite_Sound *agentite_sound_from_handle(Agentite_AssetRegistry *registry,
                                            Agentite_AssetHandle handle)
{
    if (!registry) return NULL;

    /* Verify it's a sound type */
    if (agentite_asset_get_type(registry, handle) != AGENTITE_ASSET_SOUND) {
        return NULL;
    }

    return (Agentite_Sound *)agentite_asset_get_data(registry, handle);
}

Agentite_Music *agentite_music_from_handle(Agentite_AssetRegistry *registry,
                                            Agentite_AssetHandle handle)
{
    if (!registry) return NULL;

    /* Verify it's a music type */
    if (agentite_asset_get_type(registry, handle) != AGENTITE_ASSET_MUSIC) {
        return NULL;
    }

    return (Agentite_Music *)agentite_asset_get_data(registry, handle);
}

void agentite_audio_asset_destructor(void *data, Agentite_AssetType type, void *userdata)
{
    Agentite_Audio *audio = (Agentite_Audio *)userdata;
    if (!audio || !data) return;

    switch (type) {
        case AGENTITE_ASSET_SOUND:
            agentite_sound_destroy(audio, (Agentite_Sound *)data);
            break;
        case AGENTITE_ASSET_MUSIC:
            agentite_music_destroy(audio, (Agentite_Music *)data);
            break;
        default:
            /* Not an audio asset - ignore */
            break;
    }
}
