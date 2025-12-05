#include "carbon/carbon.h"
#include "carbon/audio.h"
#include "carbon/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Sound data (fully loaded in memory)
struct Carbon_Sound {
    Uint8 *data;
    Uint32 length;
    SDL_AudioSpec spec;
};

// Music data (streamed from file)
struct Carbon_Music {
    char *filepath;
    SDL_AudioSpec spec;
    Uint8 *data;
    Uint32 length;
    bool loaded;
};

// Audio channel for mixing
typedef struct {
    Carbon_Sound *sound;
    Uint32 position;
    float volume;
    float pan;
    bool loop;
    bool active;
} AudioChannel;

// Main audio system
struct Carbon_Audio {
    SDL_AudioStream *stream;
    SDL_AudioDeviceID device_id;
    SDL_AudioSpec device_spec;

    // Mixing channels for sounds
    AudioChannel channels[CARBON_AUDIO_MAX_CHANNELS];
    int next_handle;

    // Music state
    Carbon_Music *current_music;
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
    Carbon_Audio *audio = (Carbon_Audio *)userdata;

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
    for (int ch = 0; ch < CARBON_AUDIO_MAX_CHANNELS; ch++) {
        AudioChannel *channel = &audio->channels[ch];
        if (!channel->active || !channel->sound) continue;

        Carbon_Sound *sound = channel->sound;
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
        Carbon_Music *music = audio->current_music;
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

Carbon_Audio *carbon_audio_init(void) {
    Carbon_Audio *audio = CARBON_ALLOC(Carbon_Audio);
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
        carbon_set_error_from_sdl("Failed to create audio stream");
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

void carbon_audio_shutdown(Carbon_Audio *audio) {
    if (!audio) return;

    // Stop all sounds
    carbon_sound_stop_all(audio);
    carbon_music_stop(audio);

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
static bool convert_audio_to_device(Carbon_Audio *audio, Uint8 *src_data, Uint32 src_length,
                                    SDL_AudioSpec *src_spec, Uint8 **out_data, Uint32 *out_length) {
    SDL_AudioSpec dst_spec = {
        .format = SDL_AUDIO_F32,
        .channels = 2,
        .freq = audio->device_spec.freq
    };

    // Create a temporary stream for conversion
    SDL_AudioStream *conv = SDL_CreateAudioStream(src_spec, &dst_spec);
    if (!conv) {
        carbon_set_error_from_sdl("Failed to create conversion stream");
        return false;
    }

    // Put source data
    if (!SDL_PutAudioStreamData(conv, src_data, src_length)) {
        carbon_set_error_from_sdl("Failed to put data in conversion stream");
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

Carbon_Sound *carbon_sound_load(Carbon_Audio *audio, const char *filepath) {
    if (!audio || !filepath) return NULL;

    // Load WAV file
    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV(filepath, &spec, &wav_data, &wav_length)) {
        carbon_set_error("Failed to load WAV '%s': %s", filepath, SDL_GetError());
        return NULL;
    }

    Carbon_Sound *sound = CARBON_ALLOC(Carbon_Sound);
    if (!sound) {
        SDL_free(wav_data);
        return NULL;
    }

    // Convert to device format
    if (!convert_audio_to_device(audio, wav_data, wav_length, &spec,
                                 &sound->data, &sound->length)) {
        carbon_set_error("Failed to convert audio format for '%s'", filepath);
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

Carbon_Sound *carbon_sound_load_wav_memory(Carbon_Audio *audio, const void *data, size_t size) {
    if (!audio || !data || size == 0) return NULL;

    SDL_IOStream *io = SDL_IOFromConstMem(data, size);
    if (!io) return NULL;

    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV_IO(io, true, &spec, &wav_data, &wav_length)) {
        carbon_set_error_from_sdl("Failed to load WAV from memory");
        return NULL;
    }

    Carbon_Sound *sound = CARBON_ALLOC(Carbon_Sound);
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

void carbon_sound_destroy(Carbon_Audio *audio, Carbon_Sound *sound) {
    if (!audio || !sound) return;

    // Stop any channels using this sound
    for (int i = 0; i < CARBON_AUDIO_MAX_CHANNELS; i++) {
        if (audio->channels[i].sound == sound) {
            audio->channels[i].active = false;
            audio->channels[i].sound = NULL;
        }
    }

    free(sound->data);
    free(sound);
}

Carbon_Music *carbon_music_load(Carbon_Audio *audio, const char *filepath) {
    if (!audio || !filepath) return NULL;

    // Load WAV file for music (same as sound for now)
    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_length = 0;

    if (!SDL_LoadWAV(filepath, &spec, &wav_data, &wav_length)) {
        carbon_set_error("Failed to load music '%s': %s", filepath, SDL_GetError());
        return NULL;
    }

    Carbon_Music *music = CARBON_ALLOC(Carbon_Music);
    if (!music) {
        SDL_free(wav_data);
        return NULL;
    }

    music->filepath = strdup(filepath);

    // Convert to device format
    if (!convert_audio_to_device(audio, wav_data, wav_length, &spec,
                                 &music->data, &music->length)) {
        carbon_set_error("Failed to convert music format for '%s'", filepath);
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

void carbon_music_destroy(Carbon_Audio *audio, Carbon_Music *music) {
    if (!audio || !music) return;

    // Stop if currently playing
    if (audio->current_music == music) {
        carbon_music_stop(audio);
    }

    free(music->filepath);
    free(music->data);
    free(music);
}

// Find a free channel
static int find_free_channel(Carbon_Audio *audio) {
    for (int i = 0; i < CARBON_AUDIO_MAX_CHANNELS; i++) {
        if (!audio->channels[i].active) {
            return i;
        }
    }
    // All channels busy - find oldest one and steal it
    return 0;
}

Carbon_SoundHandle carbon_sound_play(Carbon_Audio *audio, Carbon_Sound *sound) {
    return carbon_sound_play_ex(audio, sound, 1.0f, 0.0f, false);
}

Carbon_SoundHandle carbon_sound_play_ex(Carbon_Audio *audio, Carbon_Sound *sound,
                                         float volume, float pan, bool loop) {
    if (!audio || !sound) return CARBON_INVALID_SOUND_HANDLE;

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

static int handle_to_channel(Carbon_SoundHandle handle) {
    if (handle == CARBON_INVALID_SOUND_HANDLE) return -1;
    int ch = handle & 0xFF;
    if (ch < 0 || ch >= CARBON_AUDIO_MAX_CHANNELS) return -1;
    return ch;
}

void carbon_sound_stop(Carbon_Audio *audio, Carbon_SoundHandle handle) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0) {
        audio->channels[ch].active = false;
    }
}

void carbon_sound_set_volume(Carbon_Audio *audio, Carbon_SoundHandle handle, float volume) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0 && audio->channels[ch].active) {
        audio->channels[ch].volume = clampf(volume, 0.0f, 1.0f);
    }
}

void carbon_sound_set_pan(Carbon_Audio *audio, Carbon_SoundHandle handle, float pan) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0 && audio->channels[ch].active) {
        audio->channels[ch].pan = clampf(pan, -1.0f, 1.0f);
    }
}

void carbon_sound_set_loop(Carbon_Audio *audio, Carbon_SoundHandle handle, bool loop) {
    if (!audio) return;
    int ch = handle_to_channel(handle);
    if (ch >= 0 && audio->channels[ch].active) {
        audio->channels[ch].loop = loop;
    }
}

bool carbon_sound_is_playing(Carbon_Audio *audio, Carbon_SoundHandle handle) {
    if (!audio) return false;
    int ch = handle_to_channel(handle);
    if (ch >= 0) {
        return audio->channels[ch].active;
    }
    return false;
}

void carbon_sound_stop_all(Carbon_Audio *audio) {
    if (!audio) return;
    for (int i = 0; i < CARBON_AUDIO_MAX_CHANNELS; i++) {
        audio->channels[i].active = false;
    }
}

void carbon_music_play(Carbon_Audio *audio, Carbon_Music *music) {
    carbon_music_play_ex(audio, music, 1.0f, true);
}

void carbon_music_play_ex(Carbon_Audio *audio, Carbon_Music *music, float volume, bool loop) {
    if (!audio || !music) return;

    audio->current_music = music;
    audio->music_position = 0;
    audio->music_volume = clampf(volume, 0.0f, 1.0f);
    audio->music_loop = loop;
    audio->music_playing = true;
    audio->music_paused = false;
}

void carbon_music_stop(Carbon_Audio *audio) {
    if (!audio) return;
    audio->music_playing = false;
    audio->music_paused = false;
    audio->current_music = NULL;
    audio->music_position = 0;
}

void carbon_music_pause(Carbon_Audio *audio) {
    if (!audio) return;
    audio->music_paused = true;
}

void carbon_music_resume(Carbon_Audio *audio) {
    if (!audio) return;
    audio->music_paused = false;
}

void carbon_music_set_volume(Carbon_Audio *audio, float volume) {
    if (!audio) return;
    audio->music_volume = clampf(volume, 0.0f, 1.0f);
}

bool carbon_music_is_playing(Carbon_Audio *audio) {
    return audio && audio->music_playing && !audio->music_paused;
}

bool carbon_music_is_paused(Carbon_Audio *audio) {
    return audio && audio->music_playing && audio->music_paused;
}

void carbon_audio_set_master_volume(Carbon_Audio *audio, float volume) {
    if (!audio) return;
    audio->master_volume = clampf(volume, 0.0f, 1.0f);
}

float carbon_audio_get_master_volume(Carbon_Audio *audio) {
    return audio ? audio->master_volume : 0.0f;
}

void carbon_audio_set_sound_volume(Carbon_Audio *audio, float volume) {
    if (!audio) return;
    audio->sound_volume = clampf(volume, 0.0f, 1.0f);
}

float carbon_audio_get_sound_volume(Carbon_Audio *audio) {
    return audio ? audio->sound_volume : 0.0f;
}

void carbon_audio_set_music_volume(Carbon_Audio *audio, float volume) {
    if (!audio) return;
    audio->global_music_volume = clampf(volume, 0.0f, 1.0f);
}

float carbon_audio_get_music_volume(Carbon_Audio *audio) {
    return audio ? audio->global_music_volume : 0.0f;
}

void carbon_audio_update(Carbon_Audio *audio) {
    // Currently no-op - mixing is handled in callback
    // Future: could be used for streaming music from disk
    (void)audio;
}
