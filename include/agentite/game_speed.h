/**
 * @file game_speed.h
 * @brief Variable Game Speed System
 *
 * Provides variable simulation speed with pause support. Allows games to run at
 * different speeds (pause, normal, fast forward) while keeping UI responsive.
 *
 * Features:
 * - Speed multipliers (0x pause, 1x, 2x, 4x, etc.)
 * - Scaled delta time for game logic
 * - Pause state separate from speed
 * - Smooth speed transitions (optional)
 * - Speed presets with cycling
 * - Callbacks for speed/pause changes
 *
 * Usage:
 *   Agentite_GameSpeed *speed = agentite_game_speed_create();
 *
 *   // Set speed
 *   agentite_game_speed_set(speed, 2.0f);  // 2x speed
 *
 *   // In game loop:
 *   float scaled_dt = agentite_game_speed_scale_delta(speed, raw_delta);
 *   update_game_logic(scaled_dt);  // Game runs at 2x
 *   update_ui(raw_delta);          // UI runs at normal speed
 *
 *   // Pause/resume
 *   agentite_game_speed_pause(speed);
 *   agentite_game_speed_resume(speed);
 */

#ifndef AGENTITE_GAME_SPEED_H
#define AGENTITE_GAME_SPEED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================= */

/** Default speed multiplier (normal speed) */
#define AGENTITE_GAME_SPEED_DEFAULT 1.0f

/** Speed when paused */
#define AGENTITE_GAME_SPEED_PAUSED 0.0f

/** Maximum speed multiplier */
#ifndef AGENTITE_GAME_SPEED_MAX
#define AGENTITE_GAME_SPEED_MAX 16.0f
#endif

/** Minimum speed multiplier (above pause) */
#ifndef AGENTITE_GAME_SPEED_MIN
#define AGENTITE_GAME_SPEED_MIN 0.1f
#endif

/** Maximum number of speed presets */
#ifndef AGENTITE_GAME_SPEED_MAX_PRESETS
#define AGENTITE_GAME_SPEED_MAX_PRESETS 8
#endif

/* ============================================================================
 * Forward Declarations
 * ========================================================================= */

typedef struct Agentite_GameSpeed Agentite_GameSpeed;

/* ============================================================================
 * Callback Types
 * ========================================================================= */

/**
 * @brief Callback when speed changes
 *
 * @param speed Game speed system
 * @param old_speed Previous speed multiplier
 * @param new_speed New speed multiplier
 * @param userdata User data pointer
 */
typedef void (*Agentite_GameSpeedCallback)(Agentite_GameSpeed *speed,
                                          float old_speed,
                                          float new_speed,
                                          void *userdata);

/**
 * @brief Callback when pause state changes
 *
 * @param speed Game speed system
 * @param paused New pause state (true = paused)
 * @param userdata User data pointer
 */
typedef void (*Agentite_GameSpeedPauseCallback)(Agentite_GameSpeed *speed,
                                               bool paused,
                                               void *userdata);

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

/**
 * @brief Create a game speed controller
 *
 * @return New game speed system or NULL on failure
 */
Agentite_GameSpeed *agentite_game_speed_create(void);

/**
 * @brief Create a game speed controller with initial speed
 *
 * @param initial_speed Initial speed multiplier
 * @return New game speed system or NULL on failure
 */
Agentite_GameSpeed *agentite_game_speed_create_ex(float initial_speed);

/**
 * @brief Destroy a game speed controller
 *
 * @param speed Game speed system to destroy
 */
void agentite_game_speed_destroy(Agentite_GameSpeed *speed);

/* ============================================================================
 * Speed Control
 * ========================================================================= */

/**
 * @brief Set speed multiplier
 *
 * Speed is clamped to [AGENTITE_GAME_SPEED_MIN, AGENTITE_GAME_SPEED_MAX].
 * Setting speed to 0 is equivalent to pausing.
 *
 * @param speed Game speed system
 * @param multiplier Speed multiplier (1.0 = normal, 2.0 = double, etc.)
 */
void agentite_game_speed_set(Agentite_GameSpeed *speed, float multiplier);

/**
 * @brief Get current speed multiplier
 *
 * Returns 0 if paused, otherwise returns the set speed multiplier.
 *
 * @param speed Game speed system
 * @return Current effective speed multiplier
 */
float agentite_game_speed_get(Agentite_GameSpeed *speed);

/**
 * @brief Get the base speed (speed when not paused)
 *
 * @param speed Game speed system
 * @return Base speed multiplier (unaffected by pause state)
 */
float agentite_game_speed_get_base(Agentite_GameSpeed *speed);

/**
 * @brief Increase speed by a multiplier
 *
 * New speed = current * factor (e.g., factor 2.0 doubles speed)
 *
 * @param speed Game speed system
 * @param factor Multiplication factor
 */
void agentite_game_speed_multiply(Agentite_GameSpeed *speed, float factor);

/**
 * @brief Decrease speed by a divisor
 *
 * New speed = current / divisor (e.g., divisor 2.0 halves speed)
 *
 * @param speed Game speed system
 * @param divisor Division factor
 */
void agentite_game_speed_divide(Agentite_GameSpeed *speed, float divisor);

/**
 * @brief Reset speed to default (1.0x)
 *
 * Does not affect pause state.
 *
 * @param speed Game speed system
 */
void agentite_game_speed_reset(Agentite_GameSpeed *speed);

/* ============================================================================
 * Pause Control
 * ========================================================================= */

/**
 * @brief Pause the game
 *
 * @param speed Game speed system
 */
void agentite_game_speed_pause(Agentite_GameSpeed *speed);

/**
 * @brief Resume the game (unpause)
 *
 * @param speed Game speed system
 */
void agentite_game_speed_resume(Agentite_GameSpeed *speed);

/**
 * @brief Toggle pause state
 *
 * @param speed Game speed system
 */
void agentite_game_speed_toggle_pause(Agentite_GameSpeed *speed);

/**
 * @brief Check if game is paused
 *
 * @param speed Game speed system
 * @return true if paused
 */
bool agentite_game_speed_is_paused(Agentite_GameSpeed *speed);

/* ============================================================================
 * Delta Time Scaling
 * ========================================================================= */

/**
 * @brief Scale delta time by current speed
 *
 * Use this for game logic that should respect game speed.
 * Returns 0 when paused.
 *
 * @param speed Game speed system
 * @param raw_delta Raw frame delta time in seconds
 * @return Scaled delta time
 */
float agentite_game_speed_scale_delta(Agentite_GameSpeed *speed, float raw_delta);

/**
 * @brief Update smooth transitions (call each frame)
 *
 * Only needed if using smooth transitions. Updates the interpolation
 * toward target speed.
 *
 * @param speed Game speed system
 * @param delta_time Raw frame delta time
 */
void agentite_game_speed_update(Agentite_GameSpeed *speed, float delta_time);

/* ============================================================================
 * Speed Presets
 * ========================================================================= */

/**
 * @brief Set speed presets for cycling
 *
 * @param speed Game speed system
 * @param presets Array of speed multipliers
 * @param count Number of presets (max AGENTITE_GAME_SPEED_MAX_PRESETS)
 */
void agentite_game_speed_set_presets(Agentite_GameSpeed *speed,
                                    const float *presets,
                                    int count);

/**
 * @brief Set default presets (1x, 2x, 4x)
 *
 * @param speed Game speed system
 */
void agentite_game_speed_set_default_presets(Agentite_GameSpeed *speed);

/**
 * @brief Cycle to next speed preset
 *
 * Wraps around to first preset after last.
 *
 * @param speed Game speed system
 */
void agentite_game_speed_cycle(Agentite_GameSpeed *speed);

/**
 * @brief Cycle to previous speed preset
 *
 * Wraps around to last preset before first.
 *
 * @param speed Game speed system
 */
void agentite_game_speed_cycle_reverse(Agentite_GameSpeed *speed);

/**
 * @brief Set speed to a specific preset by index
 *
 * @param speed Game speed system
 * @param index Preset index (0-based)
 * @return true if preset exists
 */
bool agentite_game_speed_set_preset(Agentite_GameSpeed *speed, int index);

/**
 * @brief Get current preset index
 *
 * Returns -1 if current speed doesn't match any preset.
 *
 * @param speed Game speed system
 * @return Preset index or -1
 */
int agentite_game_speed_get_preset_index(Agentite_GameSpeed *speed);

/**
 * @brief Get number of presets
 *
 * @param speed Game speed system
 * @return Number of presets
 */
int agentite_game_speed_get_preset_count(Agentite_GameSpeed *speed);

/**
 * @brief Get preset value by index
 *
 * @param speed Game speed system
 * @param index Preset index
 * @return Preset value or 0 if invalid index
 */
float agentite_game_speed_get_preset(Agentite_GameSpeed *speed, int index);

/* ============================================================================
 * Smooth Transitions
 * ========================================================================= */

/**
 * @brief Enable/disable smooth speed transitions
 *
 * When enabled, speed changes interpolate smoothly over time.
 *
 * @param speed Game speed system
 * @param enabled true to enable smooth transitions
 */
void agentite_game_speed_set_smooth_transitions(Agentite_GameSpeed *speed, bool enabled);

/**
 * @brief Check if smooth transitions are enabled
 *
 * @param speed Game speed system
 * @return true if smooth transitions enabled
 */
bool agentite_game_speed_get_smooth_transitions(Agentite_GameSpeed *speed);

/**
 * @brief Set transition speed (how fast to interpolate)
 *
 * @param speed Game speed system
 * @param rate Interpolation rate (default 5.0, higher = faster)
 */
void agentite_game_speed_set_transition_rate(Agentite_GameSpeed *speed, float rate);

/**
 * @brief Check if currently transitioning
 *
 * @param speed Game speed system
 * @return true if speed is transitioning
 */
bool agentite_game_speed_is_transitioning(Agentite_GameSpeed *speed);

/**
 * @brief Complete transition immediately
 *
 * @param speed Game speed system
 */
void agentite_game_speed_complete_transition(Agentite_GameSpeed *speed);

/* ============================================================================
 * Speed Limits
 * ========================================================================= */

/**
 * @brief Set minimum speed limit
 *
 * @param speed Game speed system
 * @param min_speed Minimum speed (default AGENTITE_GAME_SPEED_MIN)
 */
void agentite_game_speed_set_min(Agentite_GameSpeed *speed, float min_speed);

/**
 * @brief Set maximum speed limit
 *
 * @param speed Game speed system
 * @param max_speed Maximum speed (default AGENTITE_GAME_SPEED_MAX)
 */
void agentite_game_speed_set_max(Agentite_GameSpeed *speed, float max_speed);

/**
 * @brief Get minimum speed limit
 *
 * @param speed Game speed system
 * @return Minimum speed
 */
float agentite_game_speed_get_min(Agentite_GameSpeed *speed);

/**
 * @brief Get maximum speed limit
 *
 * @param speed Game speed system
 * @return Maximum speed
 */
float agentite_game_speed_get_max(Agentite_GameSpeed *speed);

/* ============================================================================
 * Callbacks
 * ========================================================================= */

/**
 * @brief Set callback for speed changes
 *
 * @param speed Game speed system
 * @param callback Callback function (NULL to clear)
 * @param userdata User data passed to callback
 */
void agentite_game_speed_set_callback(Agentite_GameSpeed *speed,
                                     Agentite_GameSpeedCallback callback,
                                     void *userdata);

/**
 * @brief Set callback for pause state changes
 *
 * @param speed Game speed system
 * @param callback Callback function (NULL to clear)
 * @param userdata User data passed to callback
 */
void agentite_game_speed_set_pause_callback(Agentite_GameSpeed *speed,
                                           Agentite_GameSpeedPauseCallback callback,
                                           void *userdata);

/* ============================================================================
 * Statistics
 * ========================================================================= */

/**
 * @brief Get total time at current speed (scaled time)
 *
 * @param speed Game speed system
 * @return Total scaled time in seconds
 */
float agentite_game_speed_get_total_scaled_time(Agentite_GameSpeed *speed);

/**
 * @brief Get total real time elapsed
 *
 * @param speed Game speed system
 * @return Total real time in seconds
 */
float agentite_game_speed_get_total_real_time(Agentite_GameSpeed *speed);

/**
 * @brief Get total time spent paused
 *
 * @param speed Game speed system
 * @return Total paused time in seconds
 */
float agentite_game_speed_get_total_paused_time(Agentite_GameSpeed *speed);

/**
 * @brief Reset time statistics
 *
 * @param speed Game speed system
 */
void agentite_game_speed_reset_stats(Agentite_GameSpeed *speed);

/* ============================================================================
 * Utility Functions
 * ========================================================================= */

/**
 * @brief Get human-readable speed string
 *
 * Returns strings like "Paused", "1x", "2x", "0.5x", etc.
 *
 * @param speed Game speed system
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written (excluding null terminator)
 */
int agentite_game_speed_to_string(Agentite_GameSpeed *speed,
                                 char *buffer,
                                 int buffer_size);

/**
 * @brief Get speed as percentage
 *
 * @param speed Game speed system
 * @return Speed as percentage (100 = normal)
 */
int agentite_game_speed_get_percent(Agentite_GameSpeed *speed);

/**
 * @brief Check if at minimum speed
 *
 * @param speed Game speed system
 * @return true if at minimum speed
 */
bool agentite_game_speed_is_at_min(Agentite_GameSpeed *speed);

/**
 * @brief Check if at maximum speed
 *
 * @param speed Game speed system
 * @return true if at maximum speed
 */
bool agentite_game_speed_is_at_max(Agentite_GameSpeed *speed);

/**
 * @brief Check if at normal speed (1.0x)
 *
 * @param speed Game speed system
 * @return true if at normal speed
 */
bool agentite_game_speed_is_normal(Agentite_GameSpeed *speed);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_GAME_SPEED_H */
