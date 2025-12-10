#include "agentite/agentite.h"
#include "agentite/event.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

#define AGENTITE_EVENT_INITIAL_LISTENERS 8
#define AGENTITE_EVENT_DEFERRED_QUEUE_SIZE 64
#define AGENTITE_EVENT_TYPE_BUCKETS 128  /* Hash buckets for event types */

typedef struct Agentite_Listener {
    Agentite_ListenerID id;
    Agentite_EventType type;          /* AGENTITE_EVENT_NONE for "all" listeners */
    Agentite_EventCallback callback;
    void *userdata;
    bool active;
} Agentite_Listener;

struct Agentite_EventDispatcher {
    /* All listeners in a single array */
    Agentite_Listener *listeners;
    size_t listener_count;
    size_t listener_capacity;

    /* Next listener ID (monotonically increasing) */
    Agentite_ListenerID next_id;

    /* Current frame for timestamps */
    uint32_t current_frame;

    /* Deferred event queue */
    Agentite_Event *deferred_queue;
    size_t deferred_count;
    size_t deferred_capacity;

    /* Flag to prevent nested emission issues */
    bool is_emitting;
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static bool ensure_listener_capacity(Agentite_EventDispatcher *d) {
    if (d->listener_count < d->listener_capacity) {
        return true;
    }

    size_t new_capacity = d->listener_capacity == 0 ? AGENTITE_EVENT_INITIAL_LISTENERS
                                                     : d->listener_capacity * 2;
    Agentite_Listener *new_listeners = (Agentite_Listener*)realloc(d->listeners,
                                              new_capacity * sizeof(Agentite_Listener));
    if (!new_listeners) {
        return false;
    }

    d->listeners = new_listeners;
    d->listener_capacity = new_capacity;
    return true;
}

static bool ensure_deferred_capacity(Agentite_EventDispatcher *d) {
    if (d->deferred_count < d->deferred_capacity) {
        return true;
    }

    size_t new_capacity = d->deferred_capacity == 0 ? AGENTITE_EVENT_DEFERRED_QUEUE_SIZE
                                                     : d->deferred_capacity * 2;
    Agentite_Event *new_queue = (Agentite_Event*)realloc(d->deferred_queue,
                                       new_capacity * sizeof(Agentite_Event));
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

Agentite_EventDispatcher *agentite_event_dispatcher_create(void) {
    Agentite_EventDispatcher *d = AGENTITE_ALLOC(Agentite_EventDispatcher);
    if (!d) return NULL;

    d->next_id = 1;  /* ID 0 is reserved for "invalid" */
    return d;
}

void agentite_event_dispatcher_destroy(Agentite_EventDispatcher *d) {
    if (!d) return;

    free(d->listeners);
    free(d->deferred_queue);
    free(d);
}

Agentite_ListenerID agentite_event_subscribe(Agentite_EventDispatcher *d,
                                          Agentite_EventType type,
                                          Agentite_EventCallback callback,
                                          void *userdata) {
    if (!d || !callback) return 0;

    if (!ensure_listener_capacity(d)) {
        return 0;
    }

    Agentite_Listener *listener = &d->listeners[d->listener_count++];
    listener->id = d->next_id++;
    listener->type = type;
    listener->callback = callback;
    listener->userdata = userdata;
    listener->active = true;

    return listener->id;
}

Agentite_ListenerID agentite_event_subscribe_all(Agentite_EventDispatcher *d,
                                              Agentite_EventCallback callback,
                                              void *userdata) {
    return agentite_event_subscribe(d, AGENTITE_EVENT_NONE, callback, userdata);
}

void agentite_event_unsubscribe(Agentite_EventDispatcher *d, Agentite_ListenerID id) {
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

void agentite_event_emit(Agentite_EventDispatcher *d, const Agentite_Event *event) {
    if (!d || !event) return;

    /* Create mutable copy with timestamp */
    Agentite_Event e = *event;
    e.timestamp = d->current_frame;

    bool was_emitting = d->is_emitting;
    d->is_emitting = true;

    /* Iterate through all listeners */
    for (size_t i = 0; i < d->listener_count; i++) {
        Agentite_Listener *listener = &d->listeners[i];

        if (!listener->active) continue;

        /* Check if listener wants this event type (or wants all events) */
        if (listener->type == AGENTITE_EVENT_NONE || listener->type == e.type) {
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

void agentite_event_emit_deferred(Agentite_EventDispatcher *d, const Agentite_Event *event) {
    if (!d || !event) return;

    if (!ensure_deferred_capacity(d)) {
        return;
    }

    Agentite_Event *queued = &d->deferred_queue[d->deferred_count++];
    *queued = *event;
    queued->timestamp = d->current_frame;
}

void agentite_event_flush_deferred(Agentite_EventDispatcher *d) {
    if (!d || d->deferred_count == 0) return;

    /* Process all deferred events */
    /* Note: New deferred events during flush are processed immediately */
    size_t count = d->deferred_count;
    d->deferred_count = 0;

    for (size_t i = 0; i < count; i++) {
        agentite_event_emit(d, &d->deferred_queue[i]);
    }

    /* Handle any events that were deferred during flush */
    if (d->deferred_count > 0) {
        agentite_event_flush_deferred(d);
    }
}

void agentite_event_set_frame(Agentite_EventDispatcher *d, uint32_t frame) {
    if (d) {
        d->current_frame = frame;
    }
}

int agentite_event_listener_count(const Agentite_EventDispatcher *d, Agentite_EventType type) {
    if (!d) return 0;

    int count = 0;
    for (size_t i = 0; i < d->listener_count; i++) {
        if (d->listeners[i].active &&
            (d->listeners[i].type == type || d->listeners[i].type == AGENTITE_EVENT_NONE)) {
            count++;
        }
    }
    return count;
}

void agentite_event_clear_all(Agentite_EventDispatcher *d) {
    if (!d) return;

    d->listener_count = 0;
    d->deferred_count = 0;
}

/*============================================================================
 * Convenience Event Emitters
 *============================================================================*/

void agentite_event_emit_window_resize(Agentite_EventDispatcher *d, int width, int height) {
    Agentite_Event e = { .type = AGENTITE_EVENT_WINDOW_RESIZE };
    e.window_resize.width = width;
    e.window_resize.height = height;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_window_focus(Agentite_EventDispatcher *d, bool focused) {
    Agentite_Event e = { .type = focused ? AGENTITE_EVENT_WINDOW_FOCUS : AGENTITE_EVENT_WINDOW_UNFOCUS };
    e.window_focus.focused = focused;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_game_started(Agentite_EventDispatcher *d) {
    Agentite_Event e = { .type = AGENTITE_EVENT_GAME_STARTED };
    agentite_event_emit(d, &e);
}

void agentite_event_emit_game_paused(Agentite_EventDispatcher *d) {
    Agentite_Event e = { .type = AGENTITE_EVENT_GAME_PAUSED };
    agentite_event_emit(d, &e);
}

void agentite_event_emit_game_resumed(Agentite_EventDispatcher *d) {
    Agentite_Event e = { .type = AGENTITE_EVENT_GAME_RESUMED };
    agentite_event_emit(d, &e);
}

void agentite_event_emit_game_ended(Agentite_EventDispatcher *d) {
    Agentite_Event e = { .type = AGENTITE_EVENT_GAME_ENDED };
    agentite_event_emit(d, &e);
}

void agentite_event_emit_state_changed(Agentite_EventDispatcher *d, int old_state, int new_state) {
    Agentite_Event e = { .type = AGENTITE_EVENT_STATE_CHANGED };
    e.state_changed.old_state = old_state;
    e.state_changed.new_state = new_state;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_turn_started(Agentite_EventDispatcher *d, uint32_t turn) {
    Agentite_Event e = { .type = AGENTITE_EVENT_TURN_STARTED };
    e.turn.turn = turn;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_turn_ended(Agentite_EventDispatcher *d, uint32_t turn) {
    Agentite_Event e = { .type = AGENTITE_EVENT_TURN_ENDED };
    e.turn.turn = turn;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_phase_started(Agentite_EventDispatcher *d, int phase, uint32_t turn) {
    Agentite_Event e = { .type = AGENTITE_EVENT_PHASE_STARTED };
    e.phase.phase = phase;
    e.phase.turn = turn;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_phase_ended(Agentite_EventDispatcher *d, int phase, uint32_t turn) {
    Agentite_Event e = { .type = AGENTITE_EVENT_PHASE_ENDED };
    e.phase.phase = phase;
    e.phase.turn = turn;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_entity_created(Agentite_EventDispatcher *d, ecs_entity_t entity) {
    Agentite_Event e = { .type = AGENTITE_EVENT_ENTITY_CREATED };
    e.entity.entity = entity;
    e.entity.name = NULL;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_entity_destroyed(Agentite_EventDispatcher *d, ecs_entity_t entity) {
    Agentite_Event e = { .type = AGENTITE_EVENT_ENTITY_DESTROYED };
    e.entity.entity = entity;
    e.entity.name = NULL;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_selection_changed(Agentite_EventDispatcher *d, int32_t count, float x, float y) {
    Agentite_Event e = { .type = AGENTITE_EVENT_SELECTION_CHANGED };
    e.selection.count = count;
    e.selection.x = x;
    e.selection.y = y;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_selection_cleared(Agentite_EventDispatcher *d) {
    Agentite_Event e = { .type = AGENTITE_EVENT_SELECTION_CLEARED };
    e.selection.count = 0;
    e.selection.x = 0;
    e.selection.y = 0;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_resource_changed(Agentite_EventDispatcher *d, int type,
                                         int32_t old_val, int32_t new_val) {
    Agentite_Event e = { .type = AGENTITE_EVENT_RESOURCE_CHANGED };
    e.resource.resource_type = type;
    e.resource.old_value = old_val;
    e.resource.new_value = new_val;
    e.resource.delta = new_val - old_val;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_tech_researched(Agentite_EventDispatcher *d, uint32_t tech_id) {
    Agentite_Event e = { .type = AGENTITE_EVENT_TECH_RESEARCHED };
    e.tech.tech_id = tech_id;
    e.tech.tech_name = NULL;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_tech_started(Agentite_EventDispatcher *d, uint32_t tech_id) {
    Agentite_Event e = { .type = AGENTITE_EVENT_TECH_STARTED };
    e.tech.tech_id = tech_id;
    e.tech.tech_name = NULL;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_victory(Agentite_EventDispatcher *d, int victory_type, int winner_id) {
    Agentite_Event e = { .type = AGENTITE_EVENT_VICTORY_ACHIEVED };
    e.victory.victory_type = victory_type;
    e.victory.winner_id = winner_id;
    e.victory.progress = 1.0f;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_victory_progress(Agentite_EventDispatcher *d, int victory_type, float progress) {
    Agentite_Event e = { .type = AGENTITE_EVENT_VICTORY_PROGRESS };
    e.victory.victory_type = victory_type;
    e.victory.winner_id = -1;
    e.victory.progress = progress;
    agentite_event_emit(d, &e);
}

void agentite_event_emit_custom(Agentite_EventDispatcher *d, int32_t id, void *data, size_t size) {
    Agentite_Event e = { .type = AGENTITE_EVENT_CUSTOM };
    e.custom.id = id;
    e.custom.data = data;
    e.custom.size = size;
    agentite_event_emit(d, &e);
}

/*============================================================================
 * Event Type Names
 *============================================================================*/

const char *agentite_event_type_name(Agentite_EventType type) {
    switch (type) {
        case AGENTITE_EVENT_NONE:             return "NONE";

        /* Engine events */
        case AGENTITE_EVENT_WINDOW_RESIZE:    return "WINDOW_RESIZE";
        case AGENTITE_EVENT_WINDOW_FOCUS:     return "WINDOW_FOCUS";
        case AGENTITE_EVENT_WINDOW_UNFOCUS:   return "WINDOW_UNFOCUS";
        case AGENTITE_EVENT_ENGINE_SHUTDOWN:  return "ENGINE_SHUTDOWN";

        /* Game lifecycle */
        case AGENTITE_EVENT_GAME_STARTED:     return "GAME_STARTED";
        case AGENTITE_EVENT_GAME_PAUSED:      return "GAME_PAUSED";
        case AGENTITE_EVENT_GAME_RESUMED:     return "GAME_RESUMED";
        case AGENTITE_EVENT_GAME_ENDED:       return "GAME_ENDED";
        case AGENTITE_EVENT_STATE_CHANGED:    return "STATE_CHANGED";

        /* Turn-based */
        case AGENTITE_EVENT_TURN_STARTED:     return "TURN_STARTED";
        case AGENTITE_EVENT_TURN_ENDED:       return "TURN_ENDED";
        case AGENTITE_EVENT_PHASE_STARTED:    return "PHASE_STARTED";
        case AGENTITE_EVENT_PHASE_ENDED:      return "PHASE_ENDED";

        /* Entity */
        case AGENTITE_EVENT_ENTITY_CREATED:   return "ENTITY_CREATED";
        case AGENTITE_EVENT_ENTITY_DESTROYED: return "ENTITY_DESTROYED";
        case AGENTITE_EVENT_ENTITY_MODIFIED:  return "ENTITY_MODIFIED";

        /* Selection */
        case AGENTITE_EVENT_SELECTION_CHANGED: return "SELECTION_CHANGED";
        case AGENTITE_EVENT_SELECTION_CLEARED: return "SELECTION_CLEARED";

        /* Resource */
        case AGENTITE_EVENT_RESOURCE_CHANGED:   return "RESOURCE_CHANGED";
        case AGENTITE_EVENT_RESOURCE_DEPLETED:  return "RESOURCE_DEPLETED";
        case AGENTITE_EVENT_RESOURCE_THRESHOLD: return "RESOURCE_THRESHOLD";

        /* Tech */
        case AGENTITE_EVENT_TECH_RESEARCHED:  return "TECH_RESEARCHED";
        case AGENTITE_EVENT_TECH_STARTED:     return "TECH_STARTED";
        case AGENTITE_EVENT_UNLOCK_ACHIEVED:  return "UNLOCK_ACHIEVED";

        /* Victory */
        case AGENTITE_EVENT_VICTORY_ACHIEVED: return "VICTORY_ACHIEVED";
        case AGENTITE_EVENT_DEFEAT:           return "DEFEAT";
        case AGENTITE_EVENT_VICTORY_PROGRESS: return "VICTORY_PROGRESS";

        /* UI */
        case AGENTITE_EVENT_UI_BUTTON_CLICKED: return "UI_BUTTON_CLICKED";
        case AGENTITE_EVENT_UI_VALUE_CHANGED:  return "UI_VALUE_CHANGED";
        case AGENTITE_EVENT_UI_PANEL_OPENED:   return "UI_PANEL_OPENED";
        case AGENTITE_EVENT_UI_PANEL_CLOSED:   return "UI_PANEL_CLOSED";

        /* Custom */
        case AGENTITE_EVENT_CUSTOM:           return "CUSTOM";

        default:
            if (type >= AGENTITE_EVENT_CUSTOM) {
                return "CUSTOM";
            }
            return "UNKNOWN";
    }
}
