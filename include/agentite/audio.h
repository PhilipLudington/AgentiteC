/**
 * @file audio.h
 * @brief Audio system for sound effects and music playback.
 *
 * This module provides audio playback using SDL3's audio API. Features include:
 * - Sound effects: Short clips fully loaded in memory for low-latency playback
 * - Music: Longer tracks streamed from disk for memory efficiency
 * - Multiple simultaneous sounds with individual volume/pan control
 * - Master, sound, and music volume controls
 *
 * @section audio_usage Basic Usage
 * @code
 * Agentite_Audio *audio = agentite_audio_init();
 *
 * // Load assets
 * Agentite_Sound *jump = agentite_sound_load(audio, "sfx/jump.wav");
 * Agentite_Music *bgm = agentite_music_load(audio, "music/theme.ogg");
 *
 * // Play sound effect
 * agentite_sound_play(audio, jump);
 *
 * // Play background music (looping)
 * agentite_music_play_ex(audio, bgm, 0.5f, true);
 *
 * // In game loop:
 * while (running) {
 *     agentite_audio_update(audio);  // Required for music streaming
 * }
 *
 * // Cleanup
 * agentite_sound_destroy(audio, jump);
 * agentite_music_destroy(audio, bgm);
 * agentite_audio_shutdown(audio);
 * @endcode
 *
 * @section audio_thread_safety Thread Safety
 * All functions in this module are NOT thread-safe and must be called from
 * the main thread only. The internal audio callback runs on a separate thread
 * managed by SDL.
 */

#ifndef AGENTITE_AUDIO_H
#define AGENTITE_AUDIO_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/** @defgroup audio_types Types and Constants
 *  @{ */

/**
 * @brief Opaque audio system handle.
 *
 * Manages audio device, channels, and playback state.
 * Created via agentite_audio_init(), destroyed via agentite_audio_shutdown().
 */
typedef struct Agentite_Audio Agentite_Audio;

/**
 * @brief Opaque sound effect handle.
 *
 * Represents a sound effect fully loaded in memory.
 * Created via agentite_sound_load(), destroyed via agentite_sound_destroy().
 */
typedef struct Agentite_Sound Agentite_Sound;

/**
 * @brief Opaque music track handle.
 *
 * Represents a music track that streams from disk.
 * Created via agentite_music_load(), destroyed via agentite_music_destroy().
 */
typedef struct Agentite_Music Agentite_Music;

/**
 * @brief Handle for controlling a playing sound instance.
 *
 * Returned by agentite_sound_play() functions. Use to stop, adjust volume,
 * or check playback status of a specific sound instance.
 */
typedef int Agentite_SoundHandle;

/** @brief Invalid sound handle value (returned on playback failure) */
#define AGENTITE_INVALID_SOUND_HANDLE -1

/** @brief Maximum number of simultaneous sound channels */
#define AGENTITE_AUDIO_MAX_CHANNELS 32

/**
 * @brief Maximum mix buffer size in samples.
 *
 * Pre-allocated to avoid memory allocation in the audio callback.
 * 16384 samples = ~170ms at 48kHz stereo.
 */
#define AGENTITE_AUDIO_MAX_MIX_SAMPLES 16384

/** @} */ /* end of audio_types */

/** @defgroup audio_lifecycle Lifecycle Functions
 *  @{ */

/**
 * @brief Initialize the audio system.
 *
 * Opens the audio device and initializes all internal state.
 * Must be called before any other audio functions.
 *
 * @return Audio system on success, NULL on failure
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_audio_shutdown().
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_Audio *agentite_audio_init(void);

/**
 * @brief Shutdown the audio system.
 *
 * Stops all playback, closes the audio device, and frees resources.
 * All sounds and music must be destroyed before calling this.
 *
 * @param audio Audio system to shutdown (NULL is safely ignored)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_audio_shutdown(Agentite_Audio *audio);

/** @} */ /* end of audio_lifecycle */

/** @defgroup audio_sound Sound Effects
 *
 * Sound effects are short audio clips fully loaded in memory for
 * instant, low-latency playback. Multiple instances can play simultaneously.
 *
 * @{
 */

/**
 * @brief Load a sound effect from file.
 *
 * Loads the entire audio file into memory. Supports WAV, OGG, and other
 * formats via SDL.
 *
 * @param audio    Audio system (must not be NULL)
 * @param filepath Path to audio file (must not be NULL, rejects path traversal)
 *
 * @return Sound on success, NULL on failure (check agentite_get_last_error())
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_sound_destroy().
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_Sound *agentite_sound_load(Agentite_Audio *audio, const char *filepath);

/**
 * @brief Load a sound effect from memory.
 *
 * Loads a WAV file from a memory buffer.
 *
 * @param audio Audio system (must not be NULL)
 * @param data  Pointer to WAV data (must not be NULL)
 * @param size  Size of data in bytes
 *
 * @return Sound on success, NULL on failure
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_sound_destroy().
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_Sound *agentite_sound_load_wav_memory(Agentite_Audio *audio, const void *data, size_t size);

/**
 * @brief Destroy a sound effect.
 *
 * Stops any playing instances and frees the sound data.
 *
 * @param audio Audio system (must not be NULL)
 * @param sound Sound to destroy (NULL is safely ignored)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_sound_destroy(Agentite_Audio *audio, Agentite_Sound *sound);

/** @} */ /* end of audio_sound */

/** @defgroup audio_music Music
 *
 * Music tracks are streamed from disk for memory efficiency.
 * Only one music track can play at a time.
 *
 * @{
 */

/**
 * @brief Load a music track from file.
 *
 * Opens the file for streaming playback. The file remains open until
 * the music is destroyed.
 *
 * @param audio    Audio system (must not be NULL)
 * @param filepath Path to audio file (must not be NULL, rejects path traversal)
 *
 * @return Music on success, NULL on failure (check agentite_get_last_error())
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_music_destroy().
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_Music *agentite_music_load(Agentite_Audio *audio, const char *filepath);

/**
 * @brief Destroy a music track.
 *
 * Stops playback if playing and closes the file.
 *
 * @param audio Audio system (must not be NULL)
 * @param music Music to destroy (NULL is safely ignored)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_music_destroy(Agentite_Audio *audio, Agentite_Music *music);

/** @} */ /* end of audio_music */

/** @defgroup audio_sound_playback Sound Playback
 *  @{ */

/**
 * @brief Play a sound effect.
 *
 * Plays the sound at default volume (1.0), centered pan (0.0), no looping.
 *
 * @param audio Audio system (must not be NULL)
 * @param sound Sound to play (must not be NULL)
 *
 * @return Sound handle for controlling playback, or AGENTITE_INVALID_SOUND_HANDLE on failure
 */
Agentite_SoundHandle agentite_sound_play(Agentite_Audio *audio, Agentite_Sound *sound);

/**
 * @brief Play a sound effect with options.
 *
 * @param audio  Audio system (must not be NULL)
 * @param sound  Sound to play (must not be NULL)
 * @param volume Volume level (0.0 = silent, 1.0 = full volume)
 * @param pan    Stereo pan (-1.0 = left, 0.0 = center, 1.0 = right)
 * @param loop   true to loop indefinitely, false to play once
 *
 * @return Sound handle for controlling playback, or AGENTITE_INVALID_SOUND_HANDLE on failure
 */
Agentite_SoundHandle agentite_sound_play_ex(Agentite_Audio *audio, Agentite_Sound *sound,
                                            float volume, float pan, bool loop);

/**
 * @brief Stop a playing sound.
 *
 * @param audio  Audio system (must not be NULL)
 * @param handle Sound handle from agentite_sound_play()
 */
void agentite_sound_stop(Agentite_Audio *audio, Agentite_SoundHandle handle);

/**
 * @brief Set volume of a playing sound.
 *
 * @param audio  Audio system (must not be NULL)
 * @param handle Sound handle from agentite_sound_play()
 * @param volume Volume level (0.0 = silent, 1.0 = full volume)
 */
void agentite_sound_set_volume(Agentite_Audio *audio, Agentite_SoundHandle handle, float volume);

/**
 * @brief Set stereo pan of a playing sound.
 *
 * @param audio  Audio system (must not be NULL)
 * @param handle Sound handle from agentite_sound_play()
 * @param pan    Pan position (-1.0 = left, 0.0 = center, 1.0 = right)
 */
void agentite_sound_set_pan(Agentite_Audio *audio, Agentite_SoundHandle handle, float pan);

/**
 * @brief Set loop mode of a playing sound.
 *
 * @param audio  Audio system (must not be NULL)
 * @param handle Sound handle from agentite_sound_play()
 * @param loop   true to enable looping, false to disable
 */
void agentite_sound_set_loop(Agentite_Audio *audio, Agentite_SoundHandle handle, bool loop);

/**
 * @brief Check if a sound is still playing.
 *
 * @param audio  Audio system (must not be NULL)
 * @param handle Sound handle from agentite_sound_play()
 *
 * @return true if sound is playing, false if stopped or invalid handle
 */
bool agentite_sound_is_playing(const Agentite_Audio *audio, Agentite_SoundHandle handle);

/**
 * @brief Stop all currently playing sounds.
 *
 * @param audio Audio system (must not be NULL)
 */
void agentite_sound_stop_all(Agentite_Audio *audio);

/** @} */ /* end of audio_sound_playback */

/** @defgroup audio_music_playback Music Playback
 *
 * Only one music track can play at a time. Starting a new track stops
 * the current one.
 *
 * @{
 */

/**
 * @brief Play a music track.
 *
 * Plays at full volume without looping. Stops any currently playing music.
 *
 * @param audio Audio system (must not be NULL)
 * @param music Music to play (must not be NULL)
 */
void agentite_music_play(Agentite_Audio *audio, Agentite_Music *music);

/**
 * @brief Play a music track with options.
 *
 * @param audio  Audio system (must not be NULL)
 * @param music  Music to play (must not be NULL)
 * @param volume Volume level (0.0 = silent, 1.0 = full volume)
 * @param loop   true to loop indefinitely, false to play once
 */
void agentite_music_play_ex(Agentite_Audio *audio, Agentite_Music *music, float volume, bool loop);

/**
 * @brief Stop music playback.
 *
 * @param audio Audio system (must not be NULL)
 */
void agentite_music_stop(Agentite_Audio *audio);

/**
 * @brief Pause music playback.
 *
 * @param audio Audio system (must not be NULL)
 */
void agentite_music_pause(Agentite_Audio *audio);

/**
 * @brief Resume paused music playback.
 *
 * @param audio Audio system (must not be NULL)
 */
void agentite_music_resume(Agentite_Audio *audio);

/**
 * @brief Set music volume.
 *
 * @param audio  Audio system (must not be NULL)
 * @param volume Volume level (0.0 = silent, 1.0 = full volume)
 */
void agentite_music_set_volume(Agentite_Audio *audio, float volume);

/**
 * @brief Check if music is playing.
 *
 * @param audio Audio system (must not be NULL)
 *
 * @return true if music is playing (not paused), false otherwise
 */
bool agentite_music_is_playing(const Agentite_Audio *audio);

/**
 * @brief Check if music is paused.
 *
 * @param audio Audio system (must not be NULL)
 *
 * @return true if music is paused, false otherwise
 */
bool agentite_music_is_paused(const Agentite_Audio *audio);

/** @} */ /* end of audio_music_playback */

/** @defgroup audio_volume Volume Control
 *  @{ */

/**
 * @brief Set master volume (affects all audio).
 *
 * @param audio  Audio system (must not be NULL)
 * @param volume Master volume (0.0 = mute, 1.0 = full volume)
 */
void agentite_audio_set_master_volume(Agentite_Audio *audio, float volume);

/**
 * @brief Get master volume.
 *
 * @param audio Audio system (must not be NULL)
 *
 * @return Current master volume (0.0 to 1.0)
 */
float agentite_audio_get_master_volume(const Agentite_Audio *audio);

/**
 * @brief Set volume for all sound effects.
 *
 * @param audio  Audio system (must not be NULL)
 * @param volume Sound volume multiplier (0.0 = mute, 1.0 = full volume)
 */
void agentite_audio_set_sound_volume(Agentite_Audio *audio, float volume);

/**
 * @brief Get sound effects volume.
 *
 * @param audio Audio system (must not be NULL)
 *
 * @return Current sound volume (0.0 to 1.0)
 */
float agentite_audio_get_sound_volume(const Agentite_Audio *audio);

/**
 * @brief Set volume for music.
 *
 * @param audio  Audio system (must not be NULL)
 * @param volume Music volume multiplier (0.0 = mute, 1.0 = full volume)
 */
void agentite_audio_set_music_volume(Agentite_Audio *audio, float volume);

/**
 * @brief Get music volume.
 *
 * @param audio Audio system (must not be NULL)
 *
 * @return Current music volume (0.0 to 1.0)
 */
float agentite_audio_get_music_volume(const Agentite_Audio *audio);

/** @} */ /* end of audio_volume */

/** @defgroup audio_update Update
 *  @{ */

/**
 * @brief Update audio system.
 *
 * Call once per frame to update music streaming and handle any pending
 * audio operations.
 *
 * @param audio Audio system (must not be NULL)
 */
void agentite_audio_update(Agentite_Audio *audio);

/** @} */ /* end of audio_update */

/** @defgroup audio_assets Asset Handle Integration
 *
 * Integration with the Agentite asset registry for reference-counted
 * audio asset management.
 *
 * @{
 */

/** @brief Forward declaration for asset registry type (see asset.h) */
typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;

/** @brief Forward declaration for asset handle type (see asset.h) */
typedef struct Agentite_AssetHandle Agentite_AssetHandle;

/**
 * @brief Load sound and register with asset registry.
 *
 * Loads a sound effect and registers it with the asset registry for
 * automatic lifetime management via reference counting.
 *
 * @param audio    Audio system (must not be NULL)
 * @param registry Asset registry for lifetime management (must not be NULL)
 * @param path     File path (also used as asset ID, rejects path traversal)
 *
 * @return Asset handle on success, AGENTITE_INVALID_ASSET_HANDLE on failure
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_AssetHandle agentite_sound_load_asset(Agentite_Audio *audio,
                                               Agentite_AssetRegistry *registry,
                                               const char *path);

/**
 * @brief Load music and register with asset registry.
 *
 * Loads a music track and registers it with the asset registry for
 * automatic lifetime management via reference counting.
 *
 * @param audio    Audio system (must not be NULL)
 * @param registry Asset registry for lifetime management (must not be NULL)
 * @param path     File path (also used as asset ID, rejects path traversal)
 *
 * @return Asset handle on success, AGENTITE_INVALID_ASSET_HANDLE on failure
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_AssetHandle agentite_music_load_asset(Agentite_Audio *audio,
                                               Agentite_AssetRegistry *registry,
                                               const char *path);

/**
 * @brief Get sound pointer from asset handle.
 *
 * @param registry Asset registry containing the sound
 * @param handle   Asset handle from agentite_sound_load_asset()
 *
 * @return Sound pointer, or NULL if handle is invalid or not a sound
 */
Agentite_Sound *agentite_sound_from_handle(Agentite_AssetRegistry *registry,
                                           Agentite_AssetHandle handle);

/**
 * @brief Get music pointer from asset handle.
 *
 * @param registry Asset registry containing the music
 * @param handle   Asset handle from agentite_music_load_asset()
 *
 * @return Music pointer, or NULL if handle is invalid or not music
 */
Agentite_Music *agentite_music_from_handle(Agentite_AssetRegistry *registry,
                                           Agentite_AssetHandle handle);

/**
 * @brief Audio asset destructor callback for asset registry.
 *
 * Destructor function that handles both AGENTITE_ASSET_SOUND and
 * AGENTITE_ASSET_MUSIC types. Automatically destroys audio resources
 * when their reference count reaches zero.
 *
 * @param data     Audio resource pointer (cast to void*)
 * @param type     Asset type identifier (AGENTITE_ASSET_SOUND or AGENTITE_ASSET_MUSIC)
 * @param userdata Audio system pointer (must be passed when registering)
 *
 * @code
 * // Register destructor for automatic cleanup
 * agentite_asset_set_destructor(registry, agentite_audio_asset_destructor, audio);
 * @endcode
 */
void agentite_audio_asset_destructor(void *data, int type, void *userdata);

/** @} */ /* end of audio_assets */

#endif /* AGENTITE_AUDIO_H */
