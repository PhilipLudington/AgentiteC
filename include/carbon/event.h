#ifndef CARBON_EVENT_H
#define CARBON_EVENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Event Dispatcher
 *
 * A publish-subscribe event system for decoupled communication between
 * game systems. Systems can subscribe to specific event types and receive
 * callbacks when those events are emitted.
 *
 * Usage:
 *   // Create dispatcher
 *   Carbon_EventDispatcher *events = carbon_event_dispatcher_create();
 *
 *   // Subscribe to events
 *   Carbon_ListenerID id = carbon_event_subscribe(events, CARBON_EVENT_TURN_STARTED,
 *                                                  on_turn_started, userdata);
 *
 *   // Emit events
 *   carbon_event_emit_turn_started(events, turn_number);
 *
 *   // Or emit custom events
 *   Carbon_Event e = { .type = CARBON_EVENT_CUSTOM };
 *   e.custom.data = my_data;
 *   carbon_event_emit(events, &e);
 *
 *   // Cleanup
 *   carbon_event_unsubscribe(events, id);
 *   carbon_event_dispatcher_destroy(events);
 */

/* Forward declarations for ECS integration */
typedef uint64_t ecs_entity_t;

/*============================================================================
 * Event Types
 *============================================================================*/

typedef enum Carbon_EventType {
    CARBON_EVENT_NONE = 0,

    /* Engine events (1-99) */
    CARBON_EVENT_WINDOW_RESIZE,
    CARBON_EVENT_WINDOW_FOCUS,
    CARBON_EVENT_WINDOW_UNFOCUS,
    CARBON_EVENT_ENGINE_SHUTDOWN,

    /* Game lifecycle events (100-199) */
    CARBON_EVENT_GAME_STARTED = 100,
    CARBON_EVENT_GAME_PAUSED,
    CARBON_EVENT_GAME_RESUMED,
    CARBON_EVENT_GAME_ENDED,
    CARBON_EVENT_STATE_CHANGED,

    /* Turn-based events (200-299) */
    CARBON_EVENT_TURN_STARTED = 200,
    CARBON_EVENT_TURN_ENDED,
    CARBON_EVENT_PHASE_STARTED,
    CARBON_EVENT_PHASE_ENDED,

    /* Entity events (300-399) */
    CARBON_EVENT_ENTITY_CREATED = 300,
    CARBON_EVENT_ENTITY_DESTROYED,
    CARBON_EVENT_ENTITY_MODIFIED,

    /* Selection events (400-499) */
    CARBON_EVENT_SELECTION_CHANGED = 400,
    CARBON_EVENT_SELECTION_CLEARED,

    /* Resource events (500-599) */
    CARBON_EVENT_RESOURCE_CHANGED = 500,
    CARBON_EVENT_RESOURCE_DEPLETED,
    CARBON_EVENT_RESOURCE_THRESHOLD,

    /* Tech/Unlock events (600-699) */
    CARBON_EVENT_TECH_RESEARCHED = 600,
    CARBON_EVENT_TECH_STARTED,
    CARBON_EVENT_UNLOCK_ACHIEVED,

    /* Victory/Defeat events (700-799) */
    CARBON_EVENT_VICTORY_ACHIEVED = 700,
    CARBON_EVENT_DEFEAT,
    CARBON_EVENT_VICTORY_PROGRESS,

    /* UI events (800-899) */
    CARBON_EVENT_UI_BUTTON_CLICKED = 800,
    CARBON_EVENT_UI_VALUE_CHANGED,
    CARBON_EVENT_UI_PANEL_OPENED,
    CARBON_EVENT_UI_PANEL_CLOSED,

    /* Custom events (1000+) - User-defined events start here */
    CARBON_EVENT_CUSTOM = 1000,

    /* Maximum event type for internal sizing */
    CARBON_EVENT_TYPE_MAX = 2000
} Carbon_EventType;

/*============================================================================
 * Event Data Structures
 *============================================================================*/

/**
 * Event data - tagged union containing event-specific data.
 */
typedef struct Carbon_Event {
    Carbon_EventType type;
    uint32_t timestamp;     /* Frame number when emitted */

    union {
        /* Window events */
        struct {
            int width;
            int height;
        } window_resize;

        struct {
            bool focused;
        } window_focus;

        /* Game state events */
        struct {
            int old_state;
            int new_state;
        } state_changed;

        /* Turn events */
        struct {
            uint32_t turn;
        } turn;

        /* Phase events */
        struct {
            int phase;
            uint32_t turn;
        } phase;

        /* Entity events */
        struct {
            ecs_entity_t entity;
            const char *name;   /* Optional, may be NULL */
        } entity;

        /* Selection events */
        struct {
            int32_t count;      /* Number of selected items */
            float x, y;         /* Selection center (world coords) */
        } selection;

        /* Resource events */
        struct {
            int resource_type;
            int32_t old_value;
            int32_t new_value;
            int32_t delta;
        } resource;

        /* Tech events */
        struct {
            uint32_t tech_id;
            const char *tech_name;  /* Optional */
        } tech;

        /* Victory events */
        struct {
            int victory_type;
            int winner_id;
            float progress;     /* 0.0 to 1.0 */
        } victory;

        /* UI events */
        struct {
            uint32_t widget_id;
            const char *widget_name;
            union {
                int32_t int_value;
                float float_value;
                bool bool_value;
            };
        } ui;

        /* Custom event data */
        struct {
            int32_t id;         /* User-defined event ID */
            void *data;         /* User data pointer */
            size_t size;        /* Size of data (optional) */
        } custom;
    };
} Carbon_Event;

/*============================================================================
 * Event Dispatcher API
 *============================================================================*/

typedef struct Carbon_EventDispatcher Carbon_EventDispatcher;
typedef uint32_t Carbon_ListenerID;

/**
 * Callback function for event listeners.
 *
 * @param event   The event that was emitted
 * @param userdata User data passed during subscription
 */
typedef void (*Carbon_EventCallback)(const Carbon_Event *event, void *userdata);

/**
 * Create a new event dispatcher.
 *
 * @return New dispatcher or NULL on failure
 */
Carbon_EventDispatcher *carbon_event_dispatcher_create(void);

/**
 * Destroy an event dispatcher and free all resources.
 *
 * @param d Dispatcher to destroy
 */
void carbon_event_dispatcher_destroy(Carbon_EventDispatcher *d);

/**
 * Subscribe to a specific event type.
 *
 * @param d        Dispatcher
 * @param type     Event type to listen for
 * @param callback Function to call when event is emitted
 * @param userdata User data to pass to callback
 * @return Listener ID for unsubscribing, or 0 on failure
 */
Carbon_ListenerID carbon_event_subscribe(Carbon_EventDispatcher *d,
                                          Carbon_EventType type,
                                          Carbon_EventCallback callback,
                                          void *userdata);

/**
 * Subscribe to ALL event types (receives every event).
 *
 * @param d        Dispatcher
 * @param callback Function to call for all events
 * @param userdata User data to pass to callback
 * @return Listener ID for unsubscribing, or 0 on failure
 */
Carbon_ListenerID carbon_event_subscribe_all(Carbon_EventDispatcher *d,
                                              Carbon_EventCallback callback,
                                              void *userdata);

/**
 * Unsubscribe a listener by ID.
 *
 * @param d  Dispatcher
 * @param id Listener ID returned from subscribe
 */
void carbon_event_unsubscribe(Carbon_EventDispatcher *d, Carbon_ListenerID id);

/**
 * Emit an event immediately to all listeners.
 *
 * @param d     Dispatcher
 * @param event Event to emit
 */
void carbon_event_emit(Carbon_EventDispatcher *d, const Carbon_Event *event);

/**
 * Queue an event for deferred emission.
 * Use this when emitting events from within callbacks to avoid
 * modifying the listener list during iteration.
 *
 * @param d     Dispatcher
 * @param event Event to queue
 */
void carbon_event_emit_deferred(Carbon_EventDispatcher *d, const Carbon_Event *event);

/**
 * Flush all deferred events.
 * Call this at a safe point (e.g., end of frame) to emit queued events.
 *
 * @param d Dispatcher
 */
void carbon_event_flush_deferred(Carbon_EventDispatcher *d);

/**
 * Set the current frame number for event timestamps.
 *
 * @param d     Dispatcher
 * @param frame Current frame number
 */
void carbon_event_set_frame(Carbon_EventDispatcher *d, uint32_t frame);

/**
 * Get the number of listeners for a specific event type.
 *
 * @param d    Dispatcher
 * @param type Event type
 * @return Number of listeners
 */
int carbon_event_listener_count(const Carbon_EventDispatcher *d, Carbon_EventType type);

/**
 * Clear all listeners (useful for cleanup/reset).
 *
 * @param d Dispatcher
 */
void carbon_event_clear_all(Carbon_EventDispatcher *d);

/*============================================================================
 * Convenience Event Emitters
 *============================================================================*/

/* Window events */
void carbon_event_emit_window_resize(Carbon_EventDispatcher *d, int width, int height);
void carbon_event_emit_window_focus(Carbon_EventDispatcher *d, bool focused);

/* Game lifecycle events */
void carbon_event_emit_game_started(Carbon_EventDispatcher *d);
void carbon_event_emit_game_paused(Carbon_EventDispatcher *d);
void carbon_event_emit_game_resumed(Carbon_EventDispatcher *d);
void carbon_event_emit_game_ended(Carbon_EventDispatcher *d);
void carbon_event_emit_state_changed(Carbon_EventDispatcher *d, int old_state, int new_state);

/* Turn events */
void carbon_event_emit_turn_started(Carbon_EventDispatcher *d, uint32_t turn);
void carbon_event_emit_turn_ended(Carbon_EventDispatcher *d, uint32_t turn);
void carbon_event_emit_phase_started(Carbon_EventDispatcher *d, int phase, uint32_t turn);
void carbon_event_emit_phase_ended(Carbon_EventDispatcher *d, int phase, uint32_t turn);

/* Entity events */
void carbon_event_emit_entity_created(Carbon_EventDispatcher *d, ecs_entity_t entity);
void carbon_event_emit_entity_destroyed(Carbon_EventDispatcher *d, ecs_entity_t entity);

/* Selection events */
void carbon_event_emit_selection_changed(Carbon_EventDispatcher *d, int32_t count, float x, float y);
void carbon_event_emit_selection_cleared(Carbon_EventDispatcher *d);

/* Resource events */
void carbon_event_emit_resource_changed(Carbon_EventDispatcher *d, int type,
                                         int32_t old_val, int32_t new_val);

/* Tech events */
void carbon_event_emit_tech_researched(Carbon_EventDispatcher *d, uint32_t tech_id);
void carbon_event_emit_tech_started(Carbon_EventDispatcher *d, uint32_t tech_id);

/* Victory events */
void carbon_event_emit_victory(Carbon_EventDispatcher *d, int victory_type, int winner_id);
void carbon_event_emit_victory_progress(Carbon_EventDispatcher *d, int victory_type, float progress);

/* Custom events */
void carbon_event_emit_custom(Carbon_EventDispatcher *d, int32_t id, void *data, size_t size);

/*============================================================================
 * Event Type Names (for debugging)
 *============================================================================*/

/**
 * Get a human-readable name for an event type.
 *
 * @param type Event type
 * @return Static string name
 */
const char *carbon_event_type_name(Carbon_EventType type);

#endif /* CARBON_EVENT_H */
