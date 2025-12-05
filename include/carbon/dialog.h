/**
 * @file dialog.h
 * @brief Dialog / Narrative System
 *
 * Event-driven dialog queue with speaker attribution for narrative integration.
 * Supports message queuing, event-triggered dialogs, and speaker types for
 * contextual storytelling.
 *
 * Features:
 * - Queue of messages with speaker attribution
 * - Event-triggered dialogs (milestones, discoveries)
 * - Event bitmask to prevent re-triggering
 * - Speaker types (system, player, AI, custom)
 * - Printf-style formatted messages
 * - Portrait/icon support for speakers
 */

#ifndef CARBON_DIALOG_H
#define CARBON_DIALOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================= */

/** Maximum length of a dialog message */
#ifndef CARBON_DIALOG_MAX_TEXT
#define CARBON_DIALOG_MAX_TEXT 512
#endif

/** Maximum length of a speaker name */
#ifndef CARBON_DIALOG_MAX_SPEAKER_NAME
#define CARBON_DIALOG_MAX_SPEAKER_NAME 64
#endif

/** Maximum number of registered events */
#ifndef CARBON_DIALOG_MAX_EVENTS
#define CARBON_DIALOG_MAX_EVENTS 256
#endif

/** Maximum number of custom speakers */
#ifndef CARBON_DIALOG_MAX_SPEAKERS
#define CARBON_DIALOG_MAX_SPEAKERS 32
#endif

/* ============================================================================
 * Speaker Types
 * ========================================================================= */

/**
 * @brief Built-in speaker types
 */
typedef enum Carbon_SpeakerType {
    CARBON_SPEAKER_SYSTEM = 0,  /**< System/narrator messages */
    CARBON_SPEAKER_PLAYER,      /**< Player character */
    CARBON_SPEAKER_AI,          /**< AI/computer voice */
    CARBON_SPEAKER_NPC,         /**< Generic NPC */
    CARBON_SPEAKER_ENEMY,       /**< Enemy/antagonist */
    CARBON_SPEAKER_ALLY,        /**< Allied character */
    CARBON_SPEAKER_TUTORIAL,    /**< Tutorial hints */
    CARBON_SPEAKER_COUNT,

    /** User-defined speaker types start here */
    CARBON_SPEAKER_CUSTOM = 100,
} Carbon_SpeakerType;

/**
 * @brief Speaker definition for custom speakers
 */
typedef struct Carbon_Speaker {
    uint32_t id;                                /**< Unique speaker ID */
    Carbon_SpeakerType type;                    /**< Base speaker type */
    char name[CARBON_DIALOG_MAX_SPEAKER_NAME];  /**< Display name */
    uint32_t color;                             /**< Text color (ABGR format) */
    int portrait_id;                            /**< Portrait/icon ID (-1 = none) */
    void *userdata;                             /**< User-defined data */
} Carbon_Speaker;

/* ============================================================================
 * Message Priority
 * ========================================================================= */

/**
 * @brief Message priority levels
 */
typedef enum Carbon_DialogPriority {
    CARBON_DIALOG_PRIORITY_LOW = 0,     /**< Background chatter */
    CARBON_DIALOG_PRIORITY_NORMAL,      /**< Normal messages */
    CARBON_DIALOG_PRIORITY_HIGH,        /**< Important messages */
    CARBON_DIALOG_PRIORITY_CRITICAL,    /**< Must-see messages */
} Carbon_DialogPriority;

/* ============================================================================
 * Dialog Message
 * ========================================================================= */

/**
 * @brief A single dialog message
 */
typedef struct Carbon_DialogMessage {
    char text[CARBON_DIALOG_MAX_TEXT];      /**< Message text */
    Carbon_SpeakerType speaker_type;        /**< Speaker type */
    uint32_t speaker_id;                    /**< Custom speaker ID (if CUSTOM) */
    Carbon_DialogPriority priority;         /**< Message priority */
    float duration;                         /**< Display duration (0 = default) */
    float elapsed;                          /**< Time displayed so far */
    int event_id;                           /**< Triggering event ID (-1 = manual) */
    uint32_t metadata;                      /**< User-defined metadata */
} Carbon_DialogMessage;

/* ============================================================================
 * Event Definition
 * ========================================================================= */

/**
 * @brief Dialog event definition
 */
typedef struct Carbon_DialogEvent {
    int id;                                     /**< Event ID */
    char text[CARBON_DIALOG_MAX_TEXT];          /**< Message text */
    Carbon_SpeakerType speaker_type;            /**< Speaker type */
    uint32_t speaker_id;                        /**< Custom speaker ID */
    Carbon_DialogPriority priority;             /**< Message priority */
    float duration;                             /**< Display duration */
    bool repeatable;                            /**< Can trigger multiple times */
    bool active;                                /**< Is event registered */
} Carbon_DialogEvent;

/* ============================================================================
 * Forward Declarations
 * ========================================================================= */

typedef struct Carbon_DialogSystem Carbon_DialogSystem;

/* ============================================================================
 * Callback Types
 * ========================================================================= */

/**
 * @brief Callback when a message is displayed
 *
 * @param dialog Dialog system
 * @param message The message being displayed
 * @param userdata User data pointer
 */
typedef void (*Carbon_DialogDisplayCallback)(Carbon_DialogSystem *dialog,
                                              const Carbon_DialogMessage *message,
                                              void *userdata);

/**
 * @brief Callback when a message is dismissed
 *
 * @param dialog Dialog system
 * @param message The message being dismissed
 * @param userdata User data pointer
 */
typedef void (*Carbon_DialogDismissCallback)(Carbon_DialogSystem *dialog,
                                              const Carbon_DialogMessage *message,
                                              void *userdata);

/**
 * @brief Callback when an event is triggered
 *
 * @param dialog Dialog system
 * @param event_id The triggered event ID
 * @param userdata User data pointer
 */
typedef void (*Carbon_DialogEventCallback)(Carbon_DialogSystem *dialog,
                                            int event_id,
                                            void *userdata);

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

/**
 * @brief Create a dialog system
 *
 * @param max_messages Maximum messages in queue
 * @return New dialog system or NULL on failure
 */
Carbon_DialogSystem *carbon_dialog_create(int max_messages);

/**
 * @brief Destroy a dialog system
 *
 * @param dialog Dialog system to destroy
 */
void carbon_dialog_destroy(Carbon_DialogSystem *dialog);

/**
 * @brief Clear all messages from the queue
 *
 * @param dialog Dialog system
 */
void carbon_dialog_clear(Carbon_DialogSystem *dialog);

/**
 * @brief Reset all event triggers (allow re-triggering)
 *
 * @param dialog Dialog system
 */
void carbon_dialog_reset_events(Carbon_DialogSystem *dialog);

/* ============================================================================
 * Speaker Management
 * ========================================================================= */

/**
 * @brief Register a custom speaker
 *
 * @param dialog Dialog system
 * @param name Speaker display name
 * @param color Text color (ABGR format, e.g., 0xFFFFFFFF for white)
 * @param portrait_id Portrait/icon ID (-1 for none)
 * @return Speaker ID or 0 on failure
 */
uint32_t carbon_dialog_register_speaker(Carbon_DialogSystem *dialog,
                                         const char *name,
                                         uint32_t color,
                                         int portrait_id);

/**
 * @brief Register a custom speaker with extended options
 *
 * @param dialog Dialog system
 * @param speaker Speaker definition (copied)
 * @return Speaker ID or 0 on failure
 */
uint32_t carbon_dialog_register_speaker_ex(Carbon_DialogSystem *dialog,
                                            const Carbon_Speaker *speaker);

/**
 * @brief Get speaker by ID
 *
 * @param dialog Dialog system
 * @param speaker_id Speaker ID
 * @return Speaker pointer or NULL if not found
 */
const Carbon_Speaker *carbon_dialog_get_speaker(Carbon_DialogSystem *dialog,
                                                 uint32_t speaker_id);

/**
 * @brief Get speaker name (handles built-in and custom speakers)
 *
 * @param dialog Dialog system
 * @param speaker_type Speaker type
 * @param speaker_id Custom speaker ID (ignored for built-in types)
 * @return Speaker name string
 */
const char *carbon_dialog_get_speaker_name(Carbon_DialogSystem *dialog,
                                            Carbon_SpeakerType speaker_type,
                                            uint32_t speaker_id);

/**
 * @brief Get speaker color (handles built-in and custom speakers)
 *
 * @param dialog Dialog system
 * @param speaker_type Speaker type
 * @param speaker_id Custom speaker ID (ignored for built-in types)
 * @return Speaker color in ABGR format
 */
uint32_t carbon_dialog_get_speaker_color(Carbon_DialogSystem *dialog,
                                          Carbon_SpeakerType speaker_type,
                                          uint32_t speaker_id);

/**
 * @brief Set name for built-in speaker type
 *
 * @param dialog Dialog system
 * @param type Built-in speaker type
 * @param name Display name
 */
void carbon_dialog_set_speaker_name(Carbon_DialogSystem *dialog,
                                     Carbon_SpeakerType type,
                                     const char *name);

/**
 * @brief Set color for built-in speaker type
 *
 * @param dialog Dialog system
 * @param type Built-in speaker type
 * @param color Text color (ABGR format)
 */
void carbon_dialog_set_speaker_color(Carbon_DialogSystem *dialog,
                                      Carbon_SpeakerType type,
                                      uint32_t color);

/* ============================================================================
 * Message Queuing
 * ========================================================================= */

/**
 * @brief Queue a message from a speaker type
 *
 * @param dialog Dialog system
 * @param speaker_type Speaker type
 * @param text Message text
 * @return true if message was queued
 */
bool carbon_dialog_queue_message(Carbon_DialogSystem *dialog,
                                  Carbon_SpeakerType speaker_type,
                                  const char *text);

/**
 * @brief Queue a message from a custom speaker
 *
 * @param dialog Dialog system
 * @param speaker_id Custom speaker ID
 * @param text Message text
 * @return true if message was queued
 */
bool carbon_dialog_queue_message_custom(Carbon_DialogSystem *dialog,
                                         uint32_t speaker_id,
                                         const char *text);

/**
 * @brief Queue a message with full options
 *
 * @param dialog Dialog system
 * @param speaker_type Speaker type
 * @param speaker_id Custom speaker ID (if type is CUSTOM)
 * @param text Message text
 * @param priority Message priority
 * @param duration Display duration (0 = default)
 * @return true if message was queued
 */
bool carbon_dialog_queue_message_ex(Carbon_DialogSystem *dialog,
                                     Carbon_SpeakerType speaker_type,
                                     uint32_t speaker_id,
                                     const char *text,
                                     Carbon_DialogPriority priority,
                                     float duration);

/**
 * @brief Queue a printf-formatted message
 *
 * @param dialog Dialog system
 * @param speaker_type Speaker type
 * @param fmt Format string
 * @param ... Format arguments
 * @return true if message was queued
 */
bool carbon_dialog_queue_printf(Carbon_DialogSystem *dialog,
                                 Carbon_SpeakerType speaker_type,
                                 const char *fmt, ...);

/**
 * @brief Queue a printf-formatted message (va_list version)
 *
 * @param dialog Dialog system
 * @param speaker_type Speaker type
 * @param fmt Format string
 * @param args Format arguments
 * @return true if message was queued
 */
bool carbon_dialog_queue_vprintf(Carbon_DialogSystem *dialog,
                                  Carbon_SpeakerType speaker_type,
                                  const char *fmt, va_list args);

/**
 * @brief Insert a message at the front of the queue (high priority)
 *
 * @param dialog Dialog system
 * @param speaker_type Speaker type
 * @param text Message text
 * @return true if message was inserted
 */
bool carbon_dialog_insert_front(Carbon_DialogSystem *dialog,
                                 Carbon_SpeakerType speaker_type,
                                 const char *text);

/* ============================================================================
 * Event Registration
 * ========================================================================= */

/**
 * @brief Register an event-triggered dialog
 *
 * @param dialog Dialog system
 * @param event_id Unique event ID (game-defined)
 * @param speaker_type Speaker type
 * @param text Message text
 * @return true if event was registered
 */
bool carbon_dialog_register_event(Carbon_DialogSystem *dialog,
                                   int event_id,
                                   Carbon_SpeakerType speaker_type,
                                   const char *text);

/**
 * @brief Register an event with full options
 *
 * @param dialog Dialog system
 * @param event_id Unique event ID
 * @param speaker_type Speaker type
 * @param speaker_id Custom speaker ID
 * @param text Message text
 * @param priority Message priority
 * @param duration Display duration
 * @param repeatable Can trigger multiple times
 * @return true if event was registered
 */
bool carbon_dialog_register_event_ex(Carbon_DialogSystem *dialog,
                                      int event_id,
                                      Carbon_SpeakerType speaker_type,
                                      uint32_t speaker_id,
                                      const char *text,
                                      Carbon_DialogPriority priority,
                                      float duration,
                                      bool repeatable);

/**
 * @brief Unregister an event
 *
 * @param dialog Dialog system
 * @param event_id Event ID to unregister
 * @return true if event was unregistered
 */
bool carbon_dialog_unregister_event(Carbon_DialogSystem *dialog, int event_id);

/**
 * @brief Trigger an event (queues its message if not already triggered)
 *
 * @param dialog Dialog system
 * @param event_id Event ID to trigger
 * @return true if event was triggered (false if already triggered or not found)
 */
bool carbon_dialog_trigger_event(Carbon_DialogSystem *dialog, int event_id);

/**
 * @brief Check if an event has been triggered
 *
 * @param dialog Dialog system
 * @param event_id Event ID to check
 * @return true if event has been triggered
 */
bool carbon_dialog_event_triggered(Carbon_DialogSystem *dialog, int event_id);

/**
 * @brief Reset a specific event (allow re-triggering)
 *
 * @param dialog Dialog system
 * @param event_id Event ID to reset
 * @return true if event was reset
 */
bool carbon_dialog_reset_event(Carbon_DialogSystem *dialog, int event_id);

/* ============================================================================
 * Message Display
 * ========================================================================= */

/**
 * @brief Check if there's a message to display
 *
 * @param dialog Dialog system
 * @return true if there's a current message
 */
bool carbon_dialog_has_message(Carbon_DialogSystem *dialog);

/**
 * @brief Get the current message being displayed
 *
 * @param dialog Dialog system
 * @return Current message or NULL if none
 */
const Carbon_DialogMessage *carbon_dialog_current(Carbon_DialogSystem *dialog);

/**
 * @brief Advance to the next message (dismiss current)
 *
 * @param dialog Dialog system
 */
void carbon_dialog_advance(Carbon_DialogSystem *dialog);

/**
 * @brief Update dialog timing (auto-advance if duration elapsed)
 *
 * @param dialog Dialog system
 * @param delta_time Time elapsed this frame
 * @return true if a message was auto-advanced
 */
bool carbon_dialog_update(Carbon_DialogSystem *dialog, float delta_time);

/**
 * @brief Skip current message animation (instant display)
 *
 * @param dialog Dialog system
 */
void carbon_dialog_skip_animation(Carbon_DialogSystem *dialog);

/**
 * @brief Check if current message animation is complete
 *
 * @param dialog Dialog system
 * @return true if animation is complete or no message
 */
bool carbon_dialog_animation_complete(Carbon_DialogSystem *dialog);

/* ============================================================================
 * Queue State
 * ========================================================================= */

/**
 * @brief Get number of messages in queue
 *
 * @param dialog Dialog system
 * @return Message count
 */
int carbon_dialog_count(Carbon_DialogSystem *dialog);

/**
 * @brief Check if queue is empty
 *
 * @param dialog Dialog system
 * @return true if queue is empty
 */
bool carbon_dialog_is_empty(Carbon_DialogSystem *dialog);

/**
 * @brief Check if queue is full
 *
 * @param dialog Dialog system
 * @return true if queue is full
 */
bool carbon_dialog_is_full(Carbon_DialogSystem *dialog);

/**
 * @brief Get message at index
 *
 * @param dialog Dialog system
 * @param index Message index (0 = current)
 * @return Message at index or NULL if out of bounds
 */
const Carbon_DialogMessage *carbon_dialog_get(Carbon_DialogSystem *dialog, int index);

/**
 * @brief Get queue capacity
 *
 * @param dialog Dialog system
 * @return Maximum messages
 */
int carbon_dialog_capacity(Carbon_DialogSystem *dialog);

/* ============================================================================
 * Configuration
 * ========================================================================= */

/**
 * @brief Set default message duration
 *
 * @param dialog Dialog system
 * @param duration Default duration in seconds
 */
void carbon_dialog_set_default_duration(Carbon_DialogSystem *dialog, float duration);

/**
 * @brief Get default message duration
 *
 * @param dialog Dialog system
 * @return Default duration in seconds
 */
float carbon_dialog_get_default_duration(Carbon_DialogSystem *dialog);

/**
 * @brief Set text animation speed (characters per second)
 *
 * @param dialog Dialog system
 * @param chars_per_second Characters revealed per second (0 = instant)
 */
void carbon_dialog_set_text_speed(Carbon_DialogSystem *dialog, float chars_per_second);

/**
 * @brief Get text animation speed
 *
 * @param dialog Dialog system
 * @return Characters per second
 */
float carbon_dialog_get_text_speed(Carbon_DialogSystem *dialog);

/**
 * @brief Get number of characters to display (for typewriter effect)
 *
 * @param dialog Dialog system
 * @return Number of characters to show (0 if no message, -1 if complete)
 */
int carbon_dialog_get_visible_chars(Carbon_DialogSystem *dialog);

/**
 * @brief Enable/disable auto-advance
 *
 * @param dialog Dialog system
 * @param enabled true to auto-advance after duration
 */
void carbon_dialog_set_auto_advance(Carbon_DialogSystem *dialog, bool enabled);

/**
 * @brief Check if auto-advance is enabled
 *
 * @param dialog Dialog system
 * @return true if auto-advance is enabled
 */
bool carbon_dialog_get_auto_advance(Carbon_DialogSystem *dialog);

/* ============================================================================
 * Callbacks
 * ========================================================================= */

/**
 * @brief Set callback for when a message is displayed
 *
 * @param dialog Dialog system
 * @param callback Callback function (NULL to clear)
 * @param userdata User data passed to callback
 */
void carbon_dialog_set_display_callback(Carbon_DialogSystem *dialog,
                                         Carbon_DialogDisplayCallback callback,
                                         void *userdata);

/**
 * @brief Set callback for when a message is dismissed
 *
 * @param dialog Dialog system
 * @param callback Callback function (NULL to clear)
 * @param userdata User data passed to callback
 */
void carbon_dialog_set_dismiss_callback(Carbon_DialogSystem *dialog,
                                         Carbon_DialogDismissCallback callback,
                                         void *userdata);

/**
 * @brief Set callback for when an event is triggered
 *
 * @param dialog Dialog system
 * @param callback Callback function (NULL to clear)
 * @param userdata User data passed to callback
 */
void carbon_dialog_set_event_callback(Carbon_DialogSystem *dialog,
                                       Carbon_DialogEventCallback callback,
                                       void *userdata);

/* ============================================================================
 * Utility Functions
 * ========================================================================= */

/**
 * @brief Get human-readable name for a speaker type
 *
 * @param type Speaker type
 * @return Static string name
 */
const char *carbon_speaker_type_name(Carbon_SpeakerType type);

/**
 * @brief Get human-readable name for a priority level
 *
 * @param priority Priority level
 * @return Static string name
 */
const char *carbon_dialog_priority_name(Carbon_DialogPriority priority);

/**
 * @brief Get default color for a speaker type
 *
 * @param type Speaker type
 * @return Default color in ABGR format
 */
uint32_t carbon_speaker_default_color(Carbon_SpeakerType type);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_DIALOG_H */
