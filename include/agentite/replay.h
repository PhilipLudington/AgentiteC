/**
 * @file replay.h
 * @brief Replay System for Recording and Playback
 *
 * Records player commands during gameplay and allows playback with speed
 * control and seeking. Leverages the command queue system for deterministic
 * recording and playback.
 *
 * Features:
 * - Command-based recording (hooks into command system)
 * - Binary replay file format with optional compression
 * - Playback with variable speed control
 * - Seek/scrub via periodic state snapshots
 * - Replay metadata (timestamp, version, duration)
 * - UI widget for playback controls
 *
 * Usage:
 *   // Configure with serialization callbacks
 *   Agentite_ReplayConfig cfg = AGENTITE_REPLAY_CONFIG_DEFAULT;
 *   cfg.serialize = my_serialize;
 *   cfg.deserialize = my_deserialize;
 *   cfg.reset = my_reset;
 *
 *   Agentite_ReplaySystem *replay = agentite_replay_create(&cfg);
 *
 *   // Recording
 *   Agentite_ReplayMetadata meta = {0};
 *   strcpy(meta.map_name, "Level 1");
 *   agentite_replay_start_recording(replay, cmd_sys, game_state, &meta);
 *
 *   while (playing) {
 *       agentite_replay_record_frame(replay, delta_time);
 *       // ... game loop ...
 *   }
 *
 *   agentite_replay_stop_recording(replay);
 *   agentite_replay_save(replay, "game.replay");
 *
 *   // Playback
 *   agentite_replay_load(replay, "game.replay");
 *   agentite_replay_start_playback(replay, cmd_sys, game_state);
 *
 *   while (agentite_replay_is_playing(replay)) {
 *       agentite_replay_playback_frame(replay, game_state, delta_time);
 *       // ... render ...
 *   }
 *
 *   agentite_replay_destroy(replay);
 */

#ifndef AGENTITE_REPLAY_H
#define AGENTITE_REPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Replay file format version */
#define AGENTITE_REPLAY_VERSION             1

/** Minimum compatible version for loading */
#define AGENTITE_REPLAY_MIN_VERSION         1

/** Magic number for replay files ("RPLY") */
#define AGENTITE_REPLAY_MAGIC               0x52504C59

/** Default snapshot interval (frames between snapshots, ~5 sec at 60fps) */
#define AGENTITE_REPLAY_DEFAULT_SNAPSHOT_INTERVAL   300

/** Maximum path length for replay files */
#define AGENTITE_REPLAY_MAX_PATH            512

/** Maximum map name length */
#define AGENTITE_REPLAY_MAX_MAP_NAME        64

/** Maximum version string length */
#define AGENTITE_REPLAY_MAX_VERSION_STRING  32

/** Maximum timestamp length (ISO 8601) */
#define AGENTITE_REPLAY_MAX_TIMESTAMP       32

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct Agentite_ReplaySystem Agentite_ReplaySystem;
typedef struct Agentite_CommandSystem Agentite_CommandSystem;

/* Forward declare AUI_Context for widget */
struct AUI_Context;

/*============================================================================
 * Enumerations
 *============================================================================*/

/**
 * @brief Replay system state
 */
typedef enum Agentite_ReplayState {
    AGENTITE_REPLAY_IDLE = 0,       /**< Not recording or playing */
    AGENTITE_REPLAY_RECORDING,      /**< Recording commands */
    AGENTITE_REPLAY_PLAYING,        /**< Playing back commands */
    AGENTITE_REPLAY_PAUSED          /**< Playback paused */
} Agentite_ReplayState;

/**
 * @brief Replay widget display flags
 */
typedef enum Agentite_ReplayWidgetFlags {
    AGENTITE_REPLAY_WIDGET_NONE         = 0,
    AGENTITE_REPLAY_WIDGET_SHOW_TIMELINE = (1 << 0),   /**< Show timeline scrubber */
    AGENTITE_REPLAY_WIDGET_SHOW_SPEED    = (1 << 1),   /**< Show speed controls */
    AGENTITE_REPLAY_WIDGET_SHOW_TIME     = (1 << 2),   /**< Show time display */
    AGENTITE_REPLAY_WIDGET_SHOW_FRAME    = (1 << 3),   /**< Show frame counter */
    AGENTITE_REPLAY_WIDGET_COMPACT       = (1 << 4),   /**< Compact layout */
    AGENTITE_REPLAY_WIDGET_DEFAULT = (AGENTITE_REPLAY_WIDGET_SHOW_TIMELINE |
                                      AGENTITE_REPLAY_WIDGET_SHOW_SPEED |
                                      AGENTITE_REPLAY_WIDGET_SHOW_TIME)
} Agentite_ReplayWidgetFlags;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Replay metadata stored in file header
 */
typedef struct Agentite_ReplayMetadata {
    uint32_t magic;                                     /**< AGENTITE_REPLAY_MAGIC */
    int version;                                        /**< Replay format version */
    int min_compatible_version;                         /**< Min version that can load */
    char timestamp[AGENTITE_REPLAY_MAX_TIMESTAMP];      /**< Recording timestamp (ISO 8601) */
    char game_version[AGENTITE_REPLAY_MAX_VERSION_STRING]; /**< Game version string */
    char map_name[AGENTITE_REPLAY_MAX_MAP_NAME];        /**< Map/level name */
    uint64_t total_frames;                              /**< Total frames in replay */
    float total_duration;                               /**< Total duration in seconds */
    uint32_t random_seed;                               /**< RNG seed for determinism */
    int player_count;                                   /**< Number of players/factions */
} Agentite_ReplayMetadata;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * @brief Serialize game state to binary data
 *
 * Called to capture game state for initial state and periodic snapshots.
 * The callback must allocate the output buffer; the replay system will
 * free it when no longer needed.
 *
 * @param game_state    Game state to serialize
 * @param out_data      Output: allocated buffer containing serialized data
 * @param out_size      Output: size of serialized data
 * @return true on success, false on failure
 */
typedef bool (*Agentite_ReplaySerializeFunc)(void *game_state,
                                              void **out_data,
                                              size_t *out_size);

/**
 * @brief Deserialize binary data to game state
 *
 * Called to restore game state from initial state or snapshot during
 * playback and seeking.
 *
 * @param game_state    Game state to restore into
 * @param data          Serialized data buffer
 * @param size          Size of serialized data
 * @return true on success, false on failure
 */
typedef bool (*Agentite_ReplayDeserializeFunc)(void *game_state,
                                                const void *data,
                                                size_t size);

/**
 * @brief Reset game state before playback
 *
 * Called before starting playback to reset game state to a clean initial
 * condition. The metadata is provided so the game can initialize based on
 * replay parameters (e.g., random seed, map name).
 *
 * @param game_state    Game state to reset
 * @param metadata      Replay metadata
 * @return true on success, false on failure
 */
typedef bool (*Agentite_ReplayResetFunc)(void *game_state,
                                          const Agentite_ReplayMetadata *metadata);

/**
 * @brief Callback when replay playback ends
 *
 * @param replay        Replay system
 * @param userdata      User data
 */
typedef void (*Agentite_ReplayCallback)(Agentite_ReplaySystem *replay,
                                         void *userdata);

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * @brief Replay system configuration
 */
typedef struct Agentite_ReplayConfig {
    int snapshot_interval;              /**< Frames between snapshots (0 = auto) */
    int max_snapshots;                  /**< Max snapshots to keep (0 = unlimited) */
    bool compress;                      /**< Use compression for file I/O */
    Agentite_ReplaySerializeFunc serialize;     /**< State serialization callback */
    Agentite_ReplayDeserializeFunc deserialize; /**< State deserialization callback */
    Agentite_ReplayResetFunc reset;             /**< State reset callback */
} Agentite_ReplayConfig;

/** Default configuration */
#define AGENTITE_REPLAY_CONFIG_DEFAULT { \
    .snapshot_interval = AGENTITE_REPLAY_DEFAULT_SNAPSHOT_INTERVAL, \
    .max_snapshots = 0, \
    .compress = true, \
    .serialize = NULL, \
    .deserialize = NULL, \
    .reset = NULL \
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

/**
 * @brief Create a replay system
 *
 * @param config    Configuration (NULL for defaults)
 * @return New replay system or NULL on failure
 */
Agentite_ReplaySystem *agentite_replay_create(const Agentite_ReplayConfig *config);

/**
 * @brief Destroy a replay system
 *
 * Safe to call with NULL.
 *
 * @param replay    Replay system to destroy
 */
void agentite_replay_destroy(Agentite_ReplaySystem *replay);

/*============================================================================
 * Recording
 *============================================================================*/

/**
 * @brief Start recording commands
 *
 * Captures initial game state and begins recording all executed commands.
 * The command system callback will be set to capture commands; any existing
 * callback will be chained.
 *
 * @param replay        Replay system
 * @param cmd_sys       Command system to record from
 * @param game_state    Current game state (for initial snapshot)
 * @param metadata      Replay metadata (map name, version, etc.)
 * @return true on success, false on failure
 */
bool agentite_replay_start_recording(Agentite_ReplaySystem *replay,
                                      Agentite_CommandSystem *cmd_sys,
                                      void *game_state,
                                      const Agentite_ReplayMetadata *metadata);

/**
 * @brief Stop recording
 *
 * Finalizes the recording (calculates total frames, duration).
 * Call before saving to file.
 *
 * @param replay    Replay system
 */
void agentite_replay_stop_recording(Agentite_ReplaySystem *replay);

/**
 * @brief Record a frame
 *
 * Call once per frame during recording to capture frame timing.
 * Commands are automatically captured via the command system callback.
 *
 * @param replay        Replay system
 * @param delta_time    Frame delta time in seconds
 */
void agentite_replay_record_frame(Agentite_ReplaySystem *replay, float delta_time);

/**
 * @brief Force a state snapshot
 *
 * Normally snapshots are created automatically based on snapshot_interval.
 * Call this to force a snapshot at the current frame (e.g., at significant
 * game events).
 *
 * @param replay        Replay system
 * @param game_state    Current game state
 * @return true on success, false on failure
 */
bool agentite_replay_create_snapshot(Agentite_ReplaySystem *replay,
                                      void *game_state);

/*============================================================================
 * File I/O
 *============================================================================*/

/**
 * @brief Save replay to file
 *
 * @param replay    Replay system with recorded data
 * @param filepath  Output file path
 * @return true on success, false on failure
 */
bool agentite_replay_save(const Agentite_ReplaySystem *replay,
                           const char *filepath);

/**
 * @brief Load replay from file
 *
 * Loads replay data into the replay system. Call start_playback to begin
 * playing.
 *
 * @param replay    Replay system
 * @param filepath  Input file path
 * @return true on success, false on failure
 */
bool agentite_replay_load(Agentite_ReplaySystem *replay,
                           const char *filepath);

/**
 * @brief Get replay file metadata without loading
 *
 * Reads only the header to retrieve metadata.
 *
 * @param filepath  Input file path
 * @param out_meta  Output metadata
 * @return true on success, false on failure
 */
bool agentite_replay_get_file_info(const char *filepath,
                                    Agentite_ReplayMetadata *out_meta);

/**
 * @brief Check if a file is a valid replay
 *
 * @param filepath  File path to check
 * @return true if valid replay file
 */
bool agentite_replay_is_valid_file(const char *filepath);

/*============================================================================
 * Playback
 *============================================================================*/

/**
 * @brief Start playback
 *
 * Resets game state and begins playback from the start.
 *
 * @param replay        Replay system with loaded data
 * @param cmd_sys       Command system to execute commands through
 * @param game_state    Game state to restore and update
 * @return true on success, false on failure
 */
bool agentite_replay_start_playback(Agentite_ReplaySystem *replay,
                                     Agentite_CommandSystem *cmd_sys,
                                     void *game_state);

/**
 * @brief Stop playback
 *
 * @param replay    Replay system
 */
void agentite_replay_stop_playback(Agentite_ReplaySystem *replay);

/**
 * @brief Advance playback by one frame
 *
 * Retrieves commands for the current frame and executes them through
 * the command system. Call once per frame during playback.
 *
 * @param replay        Replay system
 * @param game_state    Game state (for command execution)
 * @param delta_time    Frame delta time (scaled for speed control)
 * @return Number of commands executed, or -1 on error
 */
int agentite_replay_playback_frame(Agentite_ReplaySystem *replay,
                                    void *game_state,
                                    float delta_time);

/*============================================================================
 * Playback Control
 *============================================================================*/

/**
 * @brief Pause playback
 *
 * @param replay    Replay system
 */
void agentite_replay_pause(Agentite_ReplaySystem *replay);

/**
 * @brief Resume playback
 *
 * @param replay    Replay system
 */
void agentite_replay_resume(Agentite_ReplaySystem *replay);

/**
 * @brief Toggle pause state
 *
 * @param replay    Replay system
 */
void agentite_replay_toggle_pause(Agentite_ReplaySystem *replay);

/**
 * @brief Seek to a specific frame
 *
 * Uses snapshots to quickly seek to the nearest snapshot, then fast-forwards
 * to the target frame.
 *
 * @param replay        Replay system
 * @param game_state    Game state to restore
 * @param target_frame  Target frame number
 * @return true on success, false on failure
 */
bool agentite_replay_seek(Agentite_ReplaySystem *replay,
                           void *game_state,
                           uint64_t target_frame);

/**
 * @brief Seek to a percentage of the replay
 *
 * @param replay        Replay system
 * @param game_state    Game state to restore
 * @param percent       Position (0.0 = start, 1.0 = end)
 * @return true on success, false on failure
 */
bool agentite_replay_seek_percent(Agentite_ReplaySystem *replay,
                                   void *game_state,
                                   float percent);

/**
 * @brief Step forward one frame (while paused)
 *
 * @param replay        Replay system
 * @param game_state    Game state
 * @return Number of commands executed, or -1 on error
 */
int agentite_replay_step_forward(Agentite_ReplaySystem *replay,
                                  void *game_state);

/**
 * @brief Step backward one frame (while paused)
 *
 * Requires seeking to previous frame via snapshot.
 *
 * @param replay        Replay system
 * @param game_state    Game state
 * @return true on success, false on failure
 */
bool agentite_replay_step_backward(Agentite_ReplaySystem *replay,
                                    void *game_state);

/*============================================================================
 * Speed Control
 *============================================================================*/

/**
 * @brief Set playback speed multiplier
 *
 * @param replay        Replay system
 * @param multiplier    Speed multiplier (1.0 = normal, 2.0 = double, etc.)
 */
void agentite_replay_set_speed(Agentite_ReplaySystem *replay, float multiplier);

/**
 * @brief Get current playback speed
 *
 * @param replay    Replay system
 * @return Current speed multiplier
 */
float agentite_replay_get_speed(const Agentite_ReplaySystem *replay);

/*============================================================================
 * Query State
 *============================================================================*/

/**
 * @brief Get current replay state
 *
 * @param replay    Replay system
 * @return Current state
 */
Agentite_ReplayState agentite_replay_get_state(const Agentite_ReplaySystem *replay);

/**
 * @brief Check if recording
 *
 * @param replay    Replay system
 * @return true if recording
 */
bool agentite_replay_is_recording(const Agentite_ReplaySystem *replay);

/**
 * @brief Check if playing
 *
 * @param replay    Replay system
 * @return true if playing (not paused)
 */
bool agentite_replay_is_playing(const Agentite_ReplaySystem *replay);

/**
 * @brief Check if paused
 *
 * @param replay    Replay system
 * @return true if paused
 */
bool agentite_replay_is_paused(const Agentite_ReplaySystem *replay);

/**
 * @brief Get current frame number
 *
 * @param replay    Replay system
 * @return Current frame (0-based)
 */
uint64_t agentite_replay_get_current_frame(const Agentite_ReplaySystem *replay);

/**
 * @brief Get total frame count
 *
 * @param replay    Replay system
 * @return Total frames in replay
 */
uint64_t agentite_replay_get_total_frames(const Agentite_ReplaySystem *replay);

/**
 * @brief Get current playback time
 *
 * @param replay    Replay system
 * @return Current time in seconds
 */
float agentite_replay_get_current_time(const Agentite_ReplaySystem *replay);

/**
 * @brief Get total replay duration
 *
 * @param replay    Replay system
 * @return Total duration in seconds
 */
float agentite_replay_get_total_duration(const Agentite_ReplaySystem *replay);

/**
 * @brief Get playback progress
 *
 * @param replay    Replay system
 * @return Progress (0.0 to 1.0)
 */
float agentite_replay_get_progress(const Agentite_ReplaySystem *replay);

/**
 * @brief Get replay metadata
 *
 * @param replay    Replay system
 * @return Pointer to metadata (valid while replay is loaded)
 */
const Agentite_ReplayMetadata *agentite_replay_get_metadata(const Agentite_ReplaySystem *replay);

/**
 * @brief Check if replay has data loaded
 *
 * @param replay    Replay system
 * @return true if replay data is loaded
 */
bool agentite_replay_has_data(const Agentite_ReplaySystem *replay);

/**
 * @brief Get number of snapshots
 *
 * @param replay    Replay system
 * @return Number of state snapshots
 */
int agentite_replay_get_snapshot_count(const Agentite_ReplaySystem *replay);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * @brief Set callback for playback end
 *
 * Called when playback reaches the end of the replay.
 *
 * @param replay    Replay system
 * @param callback  Callback function (NULL to clear)
 * @param userdata  User data passed to callback
 */
void agentite_replay_set_on_end(Agentite_ReplaySystem *replay,
                                 Agentite_ReplayCallback callback,
                                 void *userdata);

/**
 * @brief Set callback for seek events
 *
 * Called after a seek operation completes.
 *
 * @param replay    Replay system
 * @param callback  Callback function (NULL to clear)
 * @param userdata  User data passed to callback
 */
void agentite_replay_set_on_seek(Agentite_ReplaySystem *replay,
                                  Agentite_ReplayCallback callback,
                                  void *userdata);

/*============================================================================
 * UI Widget
 *============================================================================*/

/**
 * @brief Render replay control widget
 *
 * Renders an immediate-mode UI widget with playback controls including
 * timeline, play/pause buttons, speed selector, and time display.
 *
 * @param ui            AUI context
 * @param replay        Replay system
 * @param game_state    Game state (for seeking)
 * @param flags         Widget display flags
 * @return true if any control was interacted with
 */
bool agentite_replay_widget(struct AUI_Context *ui,
                             Agentite_ReplaySystem *replay,
                             void *game_state,
                             int flags);

/**
 * @brief Render timeline scrubber only
 *
 * @param ui            AUI context
 * @param replay        Replay system
 * @param game_state    Game state (for seeking)
 * @param width         Widget width
 * @return true if scrubber was interacted with
 */
bool agentite_replay_widget_timeline(struct AUI_Context *ui,
                                      Agentite_ReplaySystem *replay,
                                      void *game_state,
                                      float width);

/**
 * @brief Render play/pause/stop controls only
 *
 * @param ui        AUI context
 * @param replay    Replay system
 * @return true if any control was interacted with
 */
bool agentite_replay_widget_controls(struct AUI_Context *ui,
                                      Agentite_ReplaySystem *replay);

/**
 * @brief Render time display only
 *
 * @param ui        AUI context
 * @param replay    Replay system
 */
void agentite_replay_widget_time_display(struct AUI_Context *ui,
                                          const Agentite_ReplaySystem *replay);

/**
 * @brief Render speed selector only
 *
 * @param ui        AUI context
 * @param replay    Replay system
 * @return true if speed was changed
 */
bool agentite_replay_widget_speed_selector(struct AUI_Context *ui,
                                            Agentite_ReplaySystem *replay);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * @brief Clear all replay data
 *
 * Clears recorded frames and snapshots. Does not change state.
 *
 * @param replay    Replay system
 */
void agentite_replay_clear(Agentite_ReplaySystem *replay);

/**
 * @brief Format time as string
 *
 * Formats time as "MM:SS" or "HH:MM:SS" depending on duration.
 *
 * @param seconds       Time in seconds
 * @param buffer        Output buffer
 * @param buffer_size   Buffer size
 * @return Number of characters written
 */
int agentite_replay_format_time(float seconds, char *buffer, int buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_REPLAY_H */
