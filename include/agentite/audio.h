#ifndef AGENTITE_AUDIO_H
#define AGENTITE_AUDIO_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct Agentite_Audio Agentite_Audio;
typedef struct Agentite_Sound Agentite_Sound;
typedef struct Agentite_Music Agentite_Music;

// Sound handle for controlling playback
typedef int Agentite_SoundHandle;
#define AGENTITE_INVALID_SOUND_HANDLE -1

// Maximum simultaneous sound channels
#define AGENTITE_AUDIO_MAX_CHANNELS 32

// Maximum mix buffer size in samples (pre-allocated to avoid reallocation in callback)
// 16384 samples = ~170ms at 48kHz stereo
#define AGENTITE_AUDIO_MAX_MIX_SAMPLES 16384

// Audio system initialization/shutdown
Agentite_Audio *agentite_audio_init(void);
void agentite_audio_shutdown(Agentite_Audio *audio);

// Sound loading (short sound effects, fully loaded in memory)
Agentite_Sound *agentite_sound_load(Agentite_Audio *audio, const char *filepath);
Agentite_Sound *agentite_sound_load_wav_memory(Agentite_Audio *audio, const void *data, size_t size);
void agentite_sound_destroy(Agentite_Audio *audio, Agentite_Sound *sound);

// Music loading (longer tracks, streamed from disk)
Agentite_Music *agentite_music_load(Agentite_Audio *audio, const char *filepath);
void agentite_music_destroy(Agentite_Audio *audio, Agentite_Music *music);

// Sound playback (returns handle for control, or AGENTITE_INVALID_SOUND_HANDLE on failure)
Agentite_SoundHandle agentite_sound_play(Agentite_Audio *audio, Agentite_Sound *sound);
Agentite_SoundHandle agentite_sound_play_ex(Agentite_Audio *audio, Agentite_Sound *sound,
                                         float volume, float pan, bool loop);

// Sound handle control
void agentite_sound_stop(Agentite_Audio *audio, Agentite_SoundHandle handle);
void agentite_sound_set_volume(Agentite_Audio *audio, Agentite_SoundHandle handle, float volume);
void agentite_sound_set_pan(Agentite_Audio *audio, Agentite_SoundHandle handle, float pan);
void agentite_sound_set_loop(Agentite_Audio *audio, Agentite_SoundHandle handle, bool loop);
bool agentite_sound_is_playing(const Agentite_Audio *audio, Agentite_SoundHandle handle);

// Stop all playing sounds
void agentite_sound_stop_all(Agentite_Audio *audio);

// Music playback (only one music track at a time)
void agentite_music_play(Agentite_Audio *audio, Agentite_Music *music);
void agentite_music_play_ex(Agentite_Audio *audio, Agentite_Music *music, float volume, bool loop);
void agentite_music_stop(Agentite_Audio *audio);
void agentite_music_pause(Agentite_Audio *audio);
void agentite_music_resume(Agentite_Audio *audio);
void agentite_music_set_volume(Agentite_Audio *audio, float volume);
bool agentite_music_is_playing(const Agentite_Audio *audio);
bool agentite_music_is_paused(const Agentite_Audio *audio);

// Master volume control (0.0 to 1.0)
void agentite_audio_set_master_volume(Agentite_Audio *audio, float volume);
float agentite_audio_get_master_volume(const Agentite_Audio *audio);

// Separate volume controls for sounds and music
void agentite_audio_set_sound_volume(Agentite_Audio *audio, float volume);
float agentite_audio_get_sound_volume(const Agentite_Audio *audio);
void agentite_audio_set_music_volume(Agentite_Audio *audio, float volume);
float agentite_audio_get_music_volume(const Agentite_Audio *audio);

// Update (call once per frame for streaming music)
void agentite_audio_update(Agentite_Audio *audio);

/* ============================================================================
 * Asset Handle Integration
 * ============================================================================ */

/* Forward declaration */
typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;
typedef struct Agentite_AssetHandle Agentite_AssetHandle;

/**
 * Load sound and register with asset registry.
 * The sound is automatically registered with the given path and can be
 * looked up later via agentite_asset_lookup().
 *
 * @param audio    Audio system
 * @param registry Asset registry (must not be NULL)
 * @param path     File path (also used as asset ID)
 * @return Asset handle, or AGENTITE_INVALID_ASSET_HANDLE on failure
 */
Agentite_AssetHandle agentite_sound_load_asset(Agentite_Audio *audio,
                                                Agentite_AssetRegistry *registry,
                                                const char *path);

/**
 * Load music and register with asset registry.
 *
 * @param audio    Audio system
 * @param registry Asset registry (must not be NULL)
 * @param path     File path (also used as asset ID)
 * @return Asset handle, or AGENTITE_INVALID_ASSET_HANDLE on failure
 */
Agentite_AssetHandle agentite_music_load_asset(Agentite_Audio *audio,
                                                Agentite_AssetRegistry *registry,
                                                const char *path);

/**
 * Get sound pointer from asset handle.
 *
 * @param registry Asset registry
 * @param handle   Asset handle from agentite_sound_load_asset()
 * @return Sound pointer, or NULL if handle is invalid
 */
Agentite_Sound *agentite_sound_from_handle(Agentite_AssetRegistry *registry,
                                            Agentite_AssetHandle handle);

/**
 * Get music pointer from asset handle.
 *
 * @param registry Asset registry
 * @param handle   Asset handle from agentite_music_load_asset()
 * @return Music pointer, or NULL if handle is invalid
 */
Agentite_Music *agentite_music_from_handle(Agentite_AssetRegistry *registry,
                                            Agentite_AssetHandle handle);

/**
 * Audio asset destructor callback for asset registry.
 * Handles both AGENTITE_ASSET_SOUND and AGENTITE_ASSET_MUSIC types.
 * Pass the Audio system as userdata when calling agentite_asset_set_destructor().
 *
 * Example:
 *   agentite_asset_set_destructor(registry, agentite_audio_asset_destructor, audio);
 */
void agentite_audio_asset_destructor(void *data, int type, void *userdata);

#endif // AGENTITE_AUDIO_H
