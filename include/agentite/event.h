#ifndef AGENTITE_EVENT_H
#define AGENTITE_EVENT_H

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
 *   Agentite_EventDispatcher *events = agentite_event_dispatcher_create();
 *
 *   // Subscribe to events
 *   Agentite_ListenerID id = agentite_event_subscribe(events, AGENTITE_EVENT_TURN_STARTED,
 *                                                  on_turn_started, userdata);
 *
 *   // Emit events
 *   agentite_event_emit_turn_started(events, turn_number);
 *
 *   // Or emit custom events
 *   Agentite_Event e = { .type = AGENTITE_EVENT_CUSTOM };
 *   e.custom.data = my_data;
 *   agentite_event_emit(events, &e);
 *
 *   // Cleanup
 *   agentite_event_unsubscribe(events, id);
 *   agentite_event_dispatcher_destroy(events);
 */

/* Forward declarations for ECS integration */
typedef uint64_t ecs_entity_t;

/*============================================================================
 * Event Types
 *============================================================================*/

typedef enum Agentite_EventType {
    AGENTITE_EVENT_NONE = 0,

    /* Engine events (1-99) */
    AGENTITE_EVENT_WINDOW_RESIZE,
    AGENTITE_EVENT_WINDOW_FOCUS,
    AGENTITE_EVENT_WINDOW_UNFOCUS,
    AGENTITE_EVENT_ENGINE_SHUTDOWN,

    /* Game lifecycle events (100-199) */
    AGENTITE_EVENT_GAME_STARTED = 100,
    AGENTITE_EVENT_GAME_PAUSED,
    AGENTITE_EVENT_GAME_RESUMED,
    AGENTITE_EVENT_GAME_ENDED,
    AGENTITE_EVENT_STATE_CHANGED,

    /* Turn-based events (200-299) */
    AGENTITE_EVENT_TURN_STARTED = 200,
    AGENTITE_EVENT_TURN_ENDED,
    AGENTITE_EVENT_PHASE_STARTED,
    AGENTITE_EVENT_PHASE_ENDED,

    /* Entity events (300-399) */
    AGENTITE_EVENT_ENTITY_CREATED = 300,
    AGENTITE_EVENT_ENTITY_DESTROYED,
    AGENTITE_EVENT_ENTITY_MODIFIED,

    /* Selection events (400-499) */
    AGENTITE_EVENT_SELECTION_CHANGED = 400,
    AGENTITE_EVENT_SELECTION_CLEARED,

    /* Resource events (500-599) */
    AGENTITE_EVENT_RESOURCE_CHANGED = 500,
    AGENTITE_EVENT_RESOURCE_DEPLETED,
    AGENTITE_EVENT_RESOURCE_THRESHOLD,

    /* Tech/Unlock events (600-699) */
    AGENTITE_EVENT_TECH_RESEARCHED = 600,
    AGENTITE_EVENT_TECH_STARTED,
    AGENTITE_EVENT_UNLOCK_ACHIEVED,

    /* Victory/Defeat events (700-799) */
    AGENTITE_EVENT_VICTORY_ACHIEVED = 700,
    AGENTITE_EVENT_DEFEAT,
    AGENTITE_EVENT_VICTORY_PROGRESS,

    /* UI events (800-899) */
    AGENTITE_EVENT_UI_BUTTON_CLICKED = 800,
    AGENTITE_EVENT_UI_VALUE_CHANGED,
    AGENTITE_EVENT_UI_PANEL_OPENED,
    AGENTITE_EVENT_UI_PANEL_CLOSED,

    /* Custom events (1000+) - User-defined events start here */
    AGENTITE_EVENT_CUSTOM = 1000,

    /* Maximum event type for internal sizing */
    AGENTITE_EVENT_TYPE_MAX = 2000
} Agentite_EventType;

/*============================================================================
 * Event Data Structures
 *============================================================================*/

/**
 * Event data - tagged union containing event-specific data.
 */
typedef struct Agentite_Event {
    Agentite_EventType type;
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
} Agentite_Event;

/*============================================================================
 * Event Dispatcher API
 *============================================================================*/

typedef struct Agentite_EventDispatcher Agentite_EventDispatcher;
typedef uint32_t Agentite_ListenerID;

/**
 * Callback function for event listeners.
 *
 * @param event   The event that was emitted
 * @param userdata User data passed during subscription
 */
typedef void (*Agentite_EventCallback)(const Agentite_Event *event, void *userdata);

/**
 * Create a new event dispatcher.
 *
 * @return New dispatcher or NULL on failure.
 *         Caller OWNS the returned pointer and MUST call
 *         agentite_event_dispatcher_destroy() to free it.
 */
Agentite_EventDispatcher *agentite_event_dispatcher_create(void);

/**
 * Destroy an event dispatcher and free all resources.
 *
 * @param d Dispatcher to destroy
 */
void agentite_event_dispatcher_destroy(Agentite_EventDispatcher *d);

/**
 * Subscribe to a specific event type.
 *
 * @param d        Dispatcher
 * @param type     Event type to listen for
 * @param callback Function to call when event is emitted
 * @param userdata User data to pass to callback
 * @return Listener ID for unsubscribing, or 0 on failure
 */
Agentite_ListenerID agentite_event_subscribe(Agentite_EventDispatcher *d,
                                          Agentite_EventType type,
                                          Agentite_EventCallback callback,
                                          void *userdata);

/**
 * Subscribe to ALL event types (receives every event).
 *
 * @param d        Dispatcher
 * @param callback Function to call for all events
 * @param userdata User data to pass to callback
 * @return Listener ID for unsubscribing, or 0 on failure
 */
Agentite_ListenerID agentite_event_subscribe_all(Agentite_EventDispatcher *d,
                                              Agentite_EventCallback callback,
                                              void *userdata);

/**
 * Unsubscribe a listener by ID.
 *
 * @param d  Dispatcher
 * @param id Listener ID returned from subscribe
 */
void agentite_event_unsubscribe(Agentite_EventDispatcher *d, Agentite_ListenerID id);

/**
 * Emit an event immediately to all listeners.
 *
 * @param d     Dispatcher
 * @param event Event to emit
 */
void agentite_event_emit(Agentite_EventDispatcher *d, const Agentite_Event *event);

/**
 * Queue an event for deferred emission.
 * Use this when emitting events from within callbacks to avoid
 * modifying the listener list during iteration.
 *
 * @param d     Dispatcher
 * @param event Event to queue
 */
void agentite_event_emit_deferred(Agentite_EventDispatcher *d, const Agentite_Event *event);

/**
 * Flush all deferred events.
 * Call this at a safe point (e.g., end of frame) to emit queued events.
 *
 * @param d Dispatcher
 */
void agentite_event_flush_deferred(Agentite_EventDispatcher *d);

/**
 * Set the current frame number for event timestamps.
 *
 * @param d     Dispatcher
 * @param frame Current frame number
 */
void agentite_event_set_frame(Agentite_EventDispatcher *d, uint32_t frame);

/**
 * Get the number of listeners for a specific event type.
 *
 * @param d    Dispatcher
 * @param type Event type
 * @return Number of listeners
 */
int agentite_event_listener_count(const Agentite_EventDispatcher *d, Agentite_EventType type);

/**
 * Clear all listeners (useful for cleanup/reset).
 *
 * @param d Dispatcher
 */
void agentite_event_clear_all(Agentite_EventDispatcher *d);

/*============================================================================
 * Convenience Event Emitters
 *============================================================================*/

/* Window events */
void agentite_event_emit_window_resize(Agentite_EventDispatcher *d, int width, int height);
void agentite_event_emit_window_focus(Agentite_EventDispatcher *d, bool focused);

/* Game lifecycle events */
void agentite_event_emit_game_started(Agentite_EventDispatcher *d);
void agentite_event_emit_game_paused(Agentite_EventDispatcher *d);
void agentite_event_emit_game_resumed(Agentite_EventDispatcher *d);
void agentite_event_emit_game_ended(Agentite_EventDispatcher *d);
void agentite_event_emit_state_changed(Agentite_EventDispatcher *d, int old_state, int new_state);

/* Turn events */
void agentite_event_emit_turn_started(Agentite_EventDispatcher *d, uint32_t turn);
void agentite_event_emit_turn_ended(Agentite_EventDispatcher *d, uint32_t turn);
void agentite_event_emit_phase_started(Agentite_EventDispatcher *d, int phase, uint32_t turn);
void agentite_event_emit_phase_ended(Agentite_EventDispatcher *d, int phase, uint32_t turn);

/* Entity events */
void agentite_event_emit_entity_created(Agentite_EventDispatcher *d, ecs_entity_t entity);
void agentite_event_emit_entity_destroyed(Agentite_EventDispatcher *d, ecs_entity_t entity);

/* Selection events */
void agentite_event_emit_selection_changed(Agentite_EventDispatcher *d, int32_t count, float x, float y);
void agentite_event_emit_selection_cleared(Agentite_EventDispatcher *d);

/* Resource events */
void agentite_event_emit_resource_changed(Agentite_EventDispatcher *d, int type,
                                         int32_t old_val, int32_t new_val);

/* Tech events */
void agentite_event_emit_tech_researched(Agentite_EventDispatcher *d, uint32_t tech_id);
void agentite_event_emit_tech_started(Agentite_EventDispatcher *d, uint32_t tech_id);

/* Victory events */
void agentite_event_emit_victory(Agentite_EventDispatcher *d, int victory_type, int winner_id);
void agentite_event_emit_victory_progress(Agentite_EventDispatcher *d, int victory_type, float progress);

/* Custom events */
void agentite_event_emit_custom(Agentite_EventDispatcher *d, int32_t id, void *data, size_t size);

/*============================================================================
 * Event Type Names (for debugging)
 *============================================================================*/

/**
 * Get a human-readable name for an event type.
 *
 * @param type Event type
 * @return Static string name
 */
const char *agentite_event_type_name(Agentite_EventType type);

#endif /* AGENTITE_EVENT_H */
