#include "carbon/event.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

#define CARBON_EVENT_INITIAL_LISTENERS 8
#define CARBON_EVENT_DEFERRED_QUEUE_SIZE 64
#define CARBON_EVENT_TYPE_BUCKETS 128  /* Hash buckets for event types */

typedef struct Carbon_Listener {
    Carbon_ListenerID id;
    Carbon_EventType type;          /* CARBON_EVENT_NONE for "all" listeners */
    Carbon_EventCallback callback;
    void *userdata;
    bool active;
} Carbon_Listener;

struct Carbon_EventDispatcher {
    /* All listeners in a single array */
    Carbon_Listener *listeners;
    size_t listener_count;
    size_t listener_capacity;

    /* Next listener ID (monotonically increasing) */
    Carbon_ListenerID next_id;

    /* Current frame for timestamps */
    uint32_t current_frame;

    /* Deferred event queue */
    Carbon_Event *deferred_queue;
    size_t deferred_count;
    size_t deferred_capacity;

    /* Flag to prevent nested emission issues */
    bool is_emitting;
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static bool ensure_listener_capacity(Carbon_EventDispatcher *d) {
    if (d->listener_count < d->listener_capacity) {
        return true;
    }

    size_t new_capacity = d->listener_capacity == 0 ? CARBON_EVENT_INITIAL_LISTENERS
                                                     : d->listener_capacity * 2;
    Carbon_Listener *new_listeners = realloc(d->listeners,
                                              new_capacity * sizeof(Carbon_Listener));
    if (!new_listeners) {
        return false;
    }

    d->listeners = new_listeners;
    d->listener_capacity = new_capacity;
    return true;
}

static bool ensure_deferred_capacity(Carbon_EventDispatcher *d) {
    if (d->deferred_count < d->deferred_capacity) {
        return true;
    }

    size_t new_capacity = d->deferred_capacity == 0 ? CARBON_EVENT_DEFERRED_QUEUE_SIZE
                                                     : d->deferred_capacity * 2;
    Carbon_Event *new_queue = realloc(d->deferred_queue,
                                       new_capacity * sizeof(Carbon_Event));
    if (!new_queue) {
        return false;
    }

    d->deferred_queue = new_queue;
    d->deferred_capacity = new_capacity;
    return true;
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

Carbon_EventDispatcher *carbon_event_dispatcher_create(void) {
    Carbon_EventDispatcher *d = calloc(1, sizeof(Carbon_EventDispatcher));
    if (!d) return NULL;

    d->next_id = 1;  /* ID 0 is reserved for "invalid" */
    return d;
}

void carbon_event_dispatcher_destroy(Carbon_EventDispatcher *d) {
    if (!d) return;

    free(d->listeners);
    free(d->deferred_queue);
    free(d);
}

Carbon_ListenerID carbon_event_subscribe(Carbon_EventDispatcher *d,
                                          Carbon_EventType type,
                                          Carbon_EventCallback callback,
                                          void *userdata) {
    if (!d || !callback) return 0;

    if (!ensure_listener_capacity(d)) {
        return 0;
    }

    Carbon_Listener *listener = &d->listeners[d->listener_count++];
    listener->id = d->next_id++;
    listener->type = type;
    listener->callback = callback;
    listener->userdata = userdata;
    listener->active = true;

    return listener->id;
}

Carbon_ListenerID carbon_event_subscribe_all(Carbon_EventDispatcher *d,
                                              Carbon_EventCallback callback,
                                              void *userdata) {
    return carbon_event_subscribe(d, CARBON_EVENT_NONE, callback, userdata);
}

void carbon_event_unsubscribe(Carbon_EventDispatcher *d, Carbon_ListenerID id) {
    if (!d || id == 0) return;

    for (size_t i = 0; i < d->listener_count; i++) {
        if (d->listeners[i].id == id) {
            /* Mark as inactive (will be cleaned up later) */
            d->listeners[i].active = false;

            /* If not currently emitting, compact immediately */
            if (!d->is_emitting) {
                /* Swap with last and decrement count */
                if (i < d->listener_count - 1) {
                    d->listeners[i] = d->listeners[d->listener_count - 1];
                }
                d->listener_count--;
            }
            return;
        }
    }
}

void carbon_event_emit(Carbon_EventDispatcher *d, const Carbon_Event *event) {
    if (!d || !event) return;

    /* Create mutable copy with timestamp */
    Carbon_Event e = *event;
    e.timestamp = d->current_frame;

    bool was_emitting = d->is_emitting;
    d->is_emitting = true;

    /* Iterate through all listeners */
    for (size_t i = 0; i < d->listener_count; i++) {
        Carbon_Listener *listener = &d->listeners[i];

        if (!listener->active) continue;

        /* Check if listener wants this event type (or wants all events) */
        if (listener->type == CARBON_EVENT_NONE || listener->type == e.type) {
            listener->callback(&e, listener->userdata);
        }
    }

    d->is_emitting = was_emitting;

    /* Compact inactive listeners if we're done emitting */
    if (!d->is_emitting) {
        size_t write_idx = 0;
        for (size_t read_idx = 0; read_idx < d->listener_count; read_idx++) {
            if (d->listeners[read_idx].active) {
                if (write_idx != read_idx) {
                    d->listeners[write_idx] = d->listeners[read_idx];
                }
                write_idx++;
            }
        }
        d->listener_count = write_idx;
    }
}

void carbon_event_emit_deferred(Carbon_EventDispatcher *d, const Carbon_Event *event) {
    if (!d || !event) return;

    if (!ensure_deferred_capacity(d)) {
        return;
    }

    Carbon_Event *queued = &d->deferred_queue[d->deferred_count++];
    *queued = *event;
    queued->timestamp = d->current_frame;
}

void carbon_event_flush_deferred(Carbon_EventDispatcher *d) {
    if (!d || d->deferred_count == 0) return;

    /* Process all deferred events */
    /* Note: New deferred events during flush are processed immediately */
    size_t count = d->deferred_count;
    d->deferred_count = 0;

    for (size_t i = 0; i < count; i++) {
        carbon_event_emit(d, &d->deferred_queue[i]);
    }

    /* Handle any events that were deferred during flush */
    if (d->deferred_count > 0) {
        carbon_event_flush_deferred(d);
    }
}

void carbon_event_set_frame(Carbon_EventDispatcher *d, uint32_t frame) {
    if (d) {
        d->current_frame = frame;
    }
}

int carbon_event_listener_count(const Carbon_EventDispatcher *d, Carbon_EventType type) {
    if (!d) return 0;

    int count = 0;
    for (size_t i = 0; i < d->listener_count; i++) {
        if (d->listeners[i].active &&
            (d->listeners[i].type == type || d->listeners[i].type == CARBON_EVENT_NONE)) {
            count++;
        }
    }
    return count;
}

void carbon_event_clear_all(Carbon_EventDispatcher *d) {
    if (!d) return;

    d->listener_count = 0;
    d->deferred_count = 0;
}

/*============================================================================
 * Convenience Event Emitters
 *============================================================================*/

void carbon_event_emit_window_resize(Carbon_EventDispatcher *d, int width, int height) {
    Carbon_Event e = { .type = CARBON_EVENT_WINDOW_RESIZE };
    e.window_resize.width = width;
    e.window_resize.height = height;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_window_focus(Carbon_EventDispatcher *d, bool focused) {
    Carbon_Event e = { .type = focused ? CARBON_EVENT_WINDOW_FOCUS : CARBON_EVENT_WINDOW_UNFOCUS };
    e.window_focus.focused = focused;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_game_started(Carbon_EventDispatcher *d) {
    Carbon_Event e = { .type = CARBON_EVENT_GAME_STARTED };
    carbon_event_emit(d, &e);
}

void carbon_event_emit_game_paused(Carbon_EventDispatcher *d) {
    Carbon_Event e = { .type = CARBON_EVENT_GAME_PAUSED };
    carbon_event_emit(d, &e);
}

void carbon_event_emit_game_resumed(Carbon_EventDispatcher *d) {
    Carbon_Event e = { .type = CARBON_EVENT_GAME_RESUMED };
    carbon_event_emit(d, &e);
}

void carbon_event_emit_game_ended(Carbon_EventDispatcher *d) {
    Carbon_Event e = { .type = CARBON_EVENT_GAME_ENDED };
    carbon_event_emit(d, &e);
}

void carbon_event_emit_state_changed(Carbon_EventDispatcher *d, int old_state, int new_state) {
    Carbon_Event e = { .type = CARBON_EVENT_STATE_CHANGED };
    e.state_changed.old_state = old_state;
    e.state_changed.new_state = new_state;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_turn_started(Carbon_EventDispatcher *d, uint32_t turn) {
    Carbon_Event e = { .type = CARBON_EVENT_TURN_STARTED };
    e.turn.turn = turn;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_turn_ended(Carbon_EventDispatcher *d, uint32_t turn) {
    Carbon_Event e = { .type = CARBON_EVENT_TURN_ENDED };
    e.turn.turn = turn;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_phase_started(Carbon_EventDispatcher *d, int phase, uint32_t turn) {
    Carbon_Event e = { .type = CARBON_EVENT_PHASE_STARTED };
    e.phase.phase = phase;
    e.phase.turn = turn;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_phase_ended(Carbon_EventDispatcher *d, int phase, uint32_t turn) {
    Carbon_Event e = { .type = CARBON_EVENT_PHASE_ENDED };
    e.phase.phase = phase;
    e.phase.turn = turn;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_entity_created(Carbon_EventDispatcher *d, ecs_entity_t entity) {
    Carbon_Event e = { .type = CARBON_EVENT_ENTITY_CREATED };
    e.entity.entity = entity;
    e.entity.name = NULL;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_entity_destroyed(Carbon_EventDispatcher *d, ecs_entity_t entity) {
    Carbon_Event e = { .type = CARBON_EVENT_ENTITY_DESTROYED };
    e.entity.entity = entity;
    e.entity.name = NULL;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_selection_changed(Carbon_EventDispatcher *d, int32_t count, float x, float y) {
    Carbon_Event e = { .type = CARBON_EVENT_SELECTION_CHANGED };
    e.selection.count = count;
    e.selection.x = x;
    e.selection.y = y;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_selection_cleared(Carbon_EventDispatcher *d) {
    Carbon_Event e = { .type = CARBON_EVENT_SELECTION_CLEARED };
    e.selection.count = 0;
    e.selection.x = 0;
    e.selection.y = 0;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_resource_changed(Carbon_EventDispatcher *d, int type,
                                         int32_t old_val, int32_t new_val) {
    Carbon_Event e = { .type = CARBON_EVENT_RESOURCE_CHANGED };
    e.resource.resource_type = type;
    e.resource.old_value = old_val;
    e.resource.new_value = new_val;
    e.resource.delta = new_val - old_val;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_tech_researched(Carbon_EventDispatcher *d, uint32_t tech_id) {
    Carbon_Event e = { .type = CARBON_EVENT_TECH_RESEARCHED };
    e.tech.tech_id = tech_id;
    e.tech.tech_name = NULL;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_tech_started(Carbon_EventDispatcher *d, uint32_t tech_id) {
    Carbon_Event e = { .type = CARBON_EVENT_TECH_STARTED };
    e.tech.tech_id = tech_id;
    e.tech.tech_name = NULL;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_victory(Carbon_EventDispatcher *d, int victory_type, int winner_id) {
    Carbon_Event e = { .type = CARBON_EVENT_VICTORY_ACHIEVED };
    e.victory.victory_type = victory_type;
    e.victory.winner_id = winner_id;
    e.victory.progress = 1.0f;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_victory_progress(Carbon_EventDispatcher *d, int victory_type, float progress) {
    Carbon_Event e = { .type = CARBON_EVENT_VICTORY_PROGRESS };
    e.victory.victory_type = victory_type;
    e.victory.winner_id = -1;
    e.victory.progress = progress;
    carbon_event_emit(d, &e);
}

void carbon_event_emit_custom(Carbon_EventDispatcher *d, int32_t id, void *data, size_t size) {
    Carbon_Event e = { .type = CARBON_EVENT_CUSTOM };
    e.custom.id = id;
    e.custom.data = data;
    e.custom.size = size;
    carbon_event_emit(d, &e);
}

/*============================================================================
 * Event Type Names
 *============================================================================*/

const char *carbon_event_type_name(Carbon_EventType type) {
    switch (type) {
        case CARBON_EVENT_NONE:             return "NONE";

        /* Engine events */
        case CARBON_EVENT_WINDOW_RESIZE:    return "WINDOW_RESIZE";
        case CARBON_EVENT_WINDOW_FOCUS:     return "WINDOW_FOCUS";
        case CARBON_EVENT_WINDOW_UNFOCUS:   return "WINDOW_UNFOCUS";
        case CARBON_EVENT_ENGINE_SHUTDOWN:  return "ENGINE_SHUTDOWN";

        /* Game lifecycle */
        case CARBON_EVENT_GAME_STARTED:     return "GAME_STARTED";
        case CARBON_EVENT_GAME_PAUSED:      return "GAME_PAUSED";
        case CARBON_EVENT_GAME_RESUMED:     return "GAME_RESUMED";
        case CARBON_EVENT_GAME_ENDED:       return "GAME_ENDED";
        case CARBON_EVENT_STATE_CHANGED:    return "STATE_CHANGED";

        /* Turn-based */
        case CARBON_EVENT_TURN_STARTED:     return "TURN_STARTED";
        case CARBON_EVENT_TURN_ENDED:       return "TURN_ENDED";
        case CARBON_EVENT_PHASE_STARTED:    return "PHASE_STARTED";
        case CARBON_EVENT_PHASE_ENDED:      return "PHASE_ENDED";

        /* Entity */
        case CARBON_EVENT_ENTITY_CREATED:   return "ENTITY_CREATED";
        case CARBON_EVENT_ENTITY_DESTROYED: return "ENTITY_DESTROYED";
        case CARBON_EVENT_ENTITY_MODIFIED:  return "ENTITY_MODIFIED";

        /* Selection */
        case CARBON_EVENT_SELECTION_CHANGED: return "SELECTION_CHANGED";
        case CARBON_EVENT_SELECTION_CLEARED: return "SELECTION_CLEARED";

        /* Resource */
        case CARBON_EVENT_RESOURCE_CHANGED:   return "RESOURCE_CHANGED";
        case CARBON_EVENT_RESOURCE_DEPLETED:  return "RESOURCE_DEPLETED";
        case CARBON_EVENT_RESOURCE_THRESHOLD: return "RESOURCE_THRESHOLD";

        /* Tech */
        case CARBON_EVENT_TECH_RESEARCHED:  return "TECH_RESEARCHED";
        case CARBON_EVENT_TECH_STARTED:     return "TECH_STARTED";
        case CARBON_EVENT_UNLOCK_ACHIEVED:  return "UNLOCK_ACHIEVED";

        /* Victory */
        case CARBON_EVENT_VICTORY_ACHIEVED: return "VICTORY_ACHIEVED";
        case CARBON_EVENT_DEFEAT:           return "DEFEAT";
        case CARBON_EVENT_VICTORY_PROGRESS: return "VICTORY_PROGRESS";

        /* UI */
        case CARBON_EVENT_UI_BUTTON_CLICKED: return "UI_BUTTON_CLICKED";
        case CARBON_EVENT_UI_VALUE_CHANGED:  return "UI_VALUE_CHANGED";
        case CARBON_EVENT_UI_PANEL_OPENED:   return "UI_PANEL_OPENED";
        case CARBON_EVENT_UI_PANEL_CLOSED:   return "UI_PANEL_CLOSED";

        /* Custom */
        case CARBON_EVENT_CUSTOM:           return "CUSTOM";

        default:
            if (type >= CARBON_EVENT_CUSTOM) {
                return "CUSTOM";
            }
            return "UNKNOWN";
    }
}
