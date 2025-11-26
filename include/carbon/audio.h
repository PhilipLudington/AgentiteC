#ifndef CARBON_AUDIO_H
#define CARBON_AUDIO_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct Carbon_Audio Carbon_Audio;
typedef struct Carbon_Sound Carbon_Sound;
typedef struct Carbon_Music Carbon_Music;

// Sound handle for controlling playback
typedef int Carbon_SoundHandle;
#define CARBON_INVALID_SOUND_HANDLE -1

// Maximum simultaneous sound channels
#define CARBON_AUDIO_MAX_CHANNELS 32

// Audio system initialization/shutdown
Carbon_Audio *carbon_audio_init(void);
void carbon_audio_shutdown(Carbon_Audio *audio);

// Sound loading (short sound effects, fully loaded in memory)
Carbon_Sound *carbon_sound_load(Carbon_Audio *audio, const char *filepath);
Carbon_Sound *carbon_sound_load_wav_memory(Carbon_Audio *audio, const void *data, size_t size);
void carbon_sound_destroy(Carbon_Audio *audio, Carbon_Sound *sound);

// Music loading (longer tracks, streamed from disk)
Carbon_Music *carbon_music_load(Carbon_Audio *audio, const char *filepath);
void carbon_music_destroy(Carbon_Audio *audio, Carbon_Music *music);

// Sound playback (returns handle for control, or CARBON_INVALID_SOUND_HANDLE on failure)
Carbon_SoundHandle carbon_sound_play(Carbon_Audio *audio, Carbon_Sound *sound);
Carbon_SoundHandle carbon_sound_play_ex(Carbon_Audio *audio, Carbon_Sound *sound,
                                         float volume, float pan, bool loop);

// Sound handle control
void carbon_sound_stop(Carbon_Audio *audio, Carbon_SoundHandle handle);
void carbon_sound_set_volume(Carbon_Audio *audio, Carbon_SoundHandle handle, float volume);
void carbon_sound_set_pan(Carbon_Audio *audio, Carbon_SoundHandle handle, float pan);
void carbon_sound_set_loop(Carbon_Audio *audio, Carbon_SoundHandle handle, bool loop);
bool carbon_sound_is_playing(Carbon_Audio *audio, Carbon_SoundHandle handle);

// Stop all playing sounds
void carbon_sound_stop_all(Carbon_Audio *audio);

// Music playback (only one music track at a time)
void carbon_music_play(Carbon_Audio *audio, Carbon_Music *music);
void carbon_music_play_ex(Carbon_Audio *audio, Carbon_Music *music, float volume, bool loop);
void carbon_music_stop(Carbon_Audio *audio);
void carbon_music_pause(Carbon_Audio *audio);
void carbon_music_resume(Carbon_Audio *audio);
void carbon_music_set_volume(Carbon_Audio *audio, float volume);
bool carbon_music_is_playing(Carbon_Audio *audio);
bool carbon_music_is_paused(Carbon_Audio *audio);

// Master volume control (0.0 to 1.0)
void carbon_audio_set_master_volume(Carbon_Audio *audio, float volume);
float carbon_audio_get_master_volume(Carbon_Audio *audio);

// Separate volume controls for sounds and music
void carbon_audio_set_sound_volume(Carbon_Audio *audio, float volume);
float carbon_audio_get_sound_volume(Carbon_Audio *audio);
void carbon_audio_set_music_volume(Carbon_Audio *audio, float volume);
float carbon_audio_get_music_volume(Carbon_Audio *audio);

// Update (call once per frame for streaming music)
void carbon_audio_update(Carbon_Audio *audio);

#endif // CARBON_AUDIO_H
