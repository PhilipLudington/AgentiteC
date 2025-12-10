/**
 * @file dialog.c
 * @brief Dialog / Narrative System implementation
 */

#include "agentite/agentite.h"
#include "agentite/dialog.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Triggered event tracking
 */
typedef struct Agentite_EventTriggerState {
    bool triggered;     /**< Has this event been triggered */
} Agentite_EventTriggerState;

/**
 * @brief Dialog system structure
 */
struct Agentite_DialogSystem {
    /* Message queue */
    Agentite_DialogMessage *messages;     /**< Message queue (circular buffer) */
    int capacity;                       /**< Queue capacity */
    int count;                          /**< Current message count */
    int head;                           /**< Queue head index */
    int tail;                           /**< Queue tail index */

    /* Custom speakers */
    Agentite_Speaker speakers[AGENTITE_DIALOG_MAX_SPEAKERS];
    int speaker_count;
    uint32_t next_speaker_id;

    /* Built-in speaker customization */
    char builtin_names[AGENTITE_SPEAKER_COUNT][AGENTITE_DIALOG_MAX_SPEAKER_NAME];
    uint32_t builtin_colors[AGENTITE_SPEAKER_COUNT];

    /* Event definitions */
    Agentite_DialogEvent events[AGENTITE_DIALOG_MAX_EVENTS];
    Agentite_EventTriggerState event_states[AGENTITE_DIALOG_MAX_EVENTS];
    int event_count;

    /* Configuration */
    float default_duration;             /**< Default message duration */
    float text_speed;                   /**< Characters per second (0 = instant) */
    float text_elapsed;                 /**< Time since current message started */
    bool auto_advance;                  /**< Auto-advance after duration */

    /* Callbacks */
    Agentite_DialogDisplayCallback display_callback;
    void *display_userdata;
    Agentite_DialogDismissCallback dismiss_callback;
    void *dismiss_userdata;
    Agentite_DialogEventCallback event_callback;
    void *event_userdata;
};

/* ============================================================================
 * Default Colors
 * ========================================================================= */

static const uint32_t DEFAULT_SPEAKER_COLORS[AGENTITE_SPEAKER_COUNT] = {
    0xFFCCCCCC,  /* SYSTEM - light gray */
    0xFF00FF00,  /* PLAYER - green */
    0xFF00CCFF,  /* AI - cyan */
    0xFFFFFFFF,  /* NPC - white */
    0xFF0000FF,  /* ENEMY - red */
    0xFF00FF80,  /* ALLY - light green */
    0xFFFFFF00,  /* TUTORIAL - yellow */
};

static const char *DEFAULT_SPEAKER_NAMES[AGENTITE_SPEAKER_COUNT] = {
    "System",
    "Player",
    "Computer",
    "NPC",
    "Enemy",
    "Ally",
    "Tutorial",
};

/* ============================================================================
 * Helper Functions
 * ========================================================================= */

static inline int queue_next(Agentite_DialogSystem *dialog, int index) {
    return (index + 1) % dialog->capacity;
}

static inline int queue_prev(Agentite_DialogSystem *dialog, int index) {
    return (index - 1 + dialog->capacity) % dialog->capacity;
}

static Agentite_DialogEvent *find_event(Agentite_DialogSystem *dialog, int event_id) {
    for (int i = 0; i < dialog->event_count; i++) {
        if (dialog->events[i].active && dialog->events[i].id == event_id) {
            return &dialog->events[i];
        }
    }
    return NULL;
}

static int find_event_index(Agentite_DialogSystem *dialog, int event_id) {
    for (int i = 0; i < dialog->event_count; i++) {
        if (dialog->events[i].active && dialog->events[i].id == event_id) {
            return i;
        }
    }
    return -1;
}

static const Agentite_Speaker *find_speaker(Agentite_DialogSystem *dialog, uint32_t speaker_id) {
    for (int i = 0; i < dialog->speaker_count; i++) {
        if (dialog->speakers[i].id == speaker_id) {
            return &dialog->speakers[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

Agentite_DialogSystem *agentite_dialog_create(int max_messages) {
    if (max_messages <= 0) {
        agentite_set_error("Dialog: Invalid queue capacity %d", max_messages);
        return NULL;
    }

    Agentite_DialogSystem *dialog = AGENTITE_ALLOC(Agentite_DialogSystem);
    if (!dialog) {
        agentite_set_error("Dialog: Failed to allocate dialog system");
        return NULL;
    }

    dialog->messages = AGENTITE_ALLOC_ARRAY(Agentite_DialogMessage, max_messages);
    if (!dialog->messages) {
        agentite_set_error("Dialog: Failed to allocate message queue");
        free(dialog);
        return NULL;
    }

    dialog->capacity = max_messages;
    dialog->count = 0;
    dialog->head = 0;
    dialog->tail = 0;

    dialog->speaker_count = 0;
    dialog->next_speaker_id = AGENTITE_SPEAKER_CUSTOM;

    /* Initialize built-in speaker names and colors */
    for (int i = 0; i < AGENTITE_SPEAKER_COUNT; i++) {
        strncpy(dialog->builtin_names[i], DEFAULT_SPEAKER_NAMES[i],
                AGENTITE_DIALOG_MAX_SPEAKER_NAME - 1);
        dialog->builtin_colors[i] = DEFAULT_SPEAKER_COLORS[i];
    }

    dialog->event_count = 0;

    dialog->default_duration = 5.0f;
    dialog->text_speed = 0.0f;  /* Instant by default */
    dialog->text_elapsed = 0.0f;
    dialog->auto_advance = true;

    return dialog;
}

void agentite_dialog_destroy(Agentite_DialogSystem *dialog) {
    if (!dialog) return;
    free(dialog->messages);
    free(dialog);
}

void agentite_dialog_clear(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR(dialog);

    dialog->count = 0;
    dialog->head = 0;
    dialog->tail = 0;
    dialog->text_elapsed = 0.0f;
}

void agentite_dialog_reset_events(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR(dialog);

    for (int i = 0; i < AGENTITE_DIALOG_MAX_EVENTS; i++) {
        dialog->event_states[i].triggered = false;
    }
}

/* ============================================================================
 * Speaker Management
 * ========================================================================= */

uint32_t agentite_dialog_register_speaker(Agentite_DialogSystem *dialog,
                                         const char *name,
                                         uint32_t color,
                                         int portrait_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 0);
    AGENTITE_VALIDATE_PTR_RET(name, 0);

    Agentite_Speaker speaker = {0};
    speaker.type = AGENTITE_SPEAKER_CUSTOM;
    speaker.color = color;
    speaker.portrait_id = portrait_id;
    strncpy(speaker.name, name, AGENTITE_DIALOG_MAX_SPEAKER_NAME - 1);

    return agentite_dialog_register_speaker_ex(dialog, &speaker);
}

uint32_t agentite_dialog_register_speaker_ex(Agentite_DialogSystem *dialog,
                                            const Agentite_Speaker *speaker) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 0);
    AGENTITE_VALIDATE_PTR_RET(speaker, 0);

    if (dialog->speaker_count >= AGENTITE_DIALOG_MAX_SPEAKERS) {
        agentite_set_error("Dialog: Maximum speakers reached (%d)", AGENTITE_DIALOG_MAX_SPEAKERS);
        return 0;
    }

    uint32_t id = dialog->next_speaker_id++;
    Agentite_Speaker *s = &dialog->speakers[dialog->speaker_count++];
    *s = *speaker;
    s->id = id;

    return id;
}

const Agentite_Speaker *agentite_dialog_get_speaker(Agentite_DialogSystem *dialog,
                                                 uint32_t speaker_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, NULL);
    return find_speaker(dialog, speaker_id);
}

const char *agentite_dialog_get_speaker_name(Agentite_DialogSystem *dialog,
                                            Agentite_SpeakerType speaker_type,
                                            uint32_t speaker_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, "Unknown");

    if (speaker_type >= AGENTITE_SPEAKER_CUSTOM) {
        const Agentite_Speaker *speaker = find_speaker(dialog, speaker_id);
        return speaker ? speaker->name : "Unknown";
    }

    if (speaker_type >= 0 && speaker_type < AGENTITE_SPEAKER_COUNT) {
        return dialog->builtin_names[speaker_type];
    }

    return "Unknown";
}

uint32_t agentite_dialog_get_speaker_color(Agentite_DialogSystem *dialog,
                                          Agentite_SpeakerType speaker_type,
                                          uint32_t speaker_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 0xFFFFFFFF);

    if (speaker_type >= AGENTITE_SPEAKER_CUSTOM) {
        const Agentite_Speaker *speaker = find_speaker(dialog, speaker_id);
        return speaker ? speaker->color : 0xFFFFFFFF;
    }

    if (speaker_type >= 0 && speaker_type < AGENTITE_SPEAKER_COUNT) {
        return dialog->builtin_colors[speaker_type];
    }

    return 0xFFFFFFFF;
}

void agentite_dialog_set_speaker_name(Agentite_DialogSystem *dialog,
                                     Agentite_SpeakerType type,
                                     const char *name) {
    AGENTITE_VALIDATE_PTR(dialog);
    AGENTITE_VALIDATE_PTR(name);

    if (type >= 0 && type < AGENTITE_SPEAKER_COUNT) {
        strncpy(dialog->builtin_names[type], name, AGENTITE_DIALOG_MAX_SPEAKER_NAME - 1);
        dialog->builtin_names[type][AGENTITE_DIALOG_MAX_SPEAKER_NAME - 1] = '\0';
    }
}

void agentite_dialog_set_speaker_color(Agentite_DialogSystem *dialog,
                                      Agentite_SpeakerType type,
                                      uint32_t color) {
    AGENTITE_VALIDATE_PTR(dialog);

    if (type >= 0 && type < AGENTITE_SPEAKER_COUNT) {
        dialog->builtin_colors[type] = color;
    }
}

/* ============================================================================
 * Message Queuing
 * ========================================================================= */

bool agentite_dialog_queue_message(Agentite_DialogSystem *dialog,
                                  Agentite_SpeakerType speaker_type,
                                  const char *text) {
    return agentite_dialog_queue_message_ex(dialog, speaker_type, 0, text,
                                           AGENTITE_DIALOG_PRIORITY_NORMAL, 0.0f);
}

bool agentite_dialog_queue_message_custom(Agentite_DialogSystem *dialog,
                                         uint32_t speaker_id,
                                         const char *text) {
    return agentite_dialog_queue_message_ex(dialog, AGENTITE_SPEAKER_CUSTOM, speaker_id, text,
                                           AGENTITE_DIALOG_PRIORITY_NORMAL, 0.0f);
}

bool agentite_dialog_queue_message_ex(Agentite_DialogSystem *dialog,
                                     Agentite_SpeakerType speaker_type,
                                     uint32_t speaker_id,
                                     const char *text,
                                     Agentite_DialogPriority priority,
                                     float duration) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);
    AGENTITE_VALIDATE_PTR_RET(text, false);

    if (dialog->count >= dialog->capacity) {
        agentite_set_error("Dialog: Message queue full");
        return false;
    }

    Agentite_DialogMessage *msg = &dialog->messages[dialog->tail];
    memset(msg, 0, sizeof(Agentite_DialogMessage));

    strncpy(msg->text, text, AGENTITE_DIALOG_MAX_TEXT - 1);
    msg->text[AGENTITE_DIALOG_MAX_TEXT - 1] = '\0';

    msg->speaker_type = speaker_type;
    msg->speaker_id = speaker_id;
    msg->priority = priority;
    msg->duration = (duration > 0.0f) ? duration : dialog->default_duration;
    msg->elapsed = 0.0f;
    msg->event_id = -1;
    msg->metadata = 0;

    dialog->tail = queue_next(dialog, dialog->tail);
    dialog->count++;

    /* Fire display callback if this is the first message */
    if (dialog->count == 1 && dialog->display_callback) {
        dialog->text_elapsed = 0.0f;
        dialog->display_callback(dialog, msg, dialog->display_userdata);
    }

    return true;
}

bool agentite_dialog_queue_printf(Agentite_DialogSystem *dialog,
                                 Agentite_SpeakerType speaker_type,
                                 const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    bool result = agentite_dialog_queue_vprintf(dialog, speaker_type, fmt, args);
    va_end(args);
    return result;
}

bool agentite_dialog_queue_vprintf(Agentite_DialogSystem *dialog,
                                  Agentite_SpeakerType speaker_type,
                                  const char *fmt, va_list args) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);
    AGENTITE_VALIDATE_PTR_RET(fmt, false);

    char buffer[AGENTITE_DIALOG_MAX_TEXT];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    return agentite_dialog_queue_message(dialog, speaker_type, buffer);
}

bool agentite_dialog_insert_front(Agentite_DialogSystem *dialog,
                                 Agentite_SpeakerType speaker_type,
                                 const char *text) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);
    AGENTITE_VALIDATE_PTR_RET(text, false);

    if (dialog->count >= dialog->capacity) {
        agentite_set_error("Dialog: Message queue full");
        return false;
    }

    /* Insert at head (before current message) */
    dialog->head = queue_prev(dialog, dialog->head);

    Agentite_DialogMessage *msg = &dialog->messages[dialog->head];
    memset(msg, 0, sizeof(Agentite_DialogMessage));

    strncpy(msg->text, text, AGENTITE_DIALOG_MAX_TEXT - 1);
    msg->text[AGENTITE_DIALOG_MAX_TEXT - 1] = '\0';

    msg->speaker_type = speaker_type;
    msg->speaker_id = 0;
    msg->priority = AGENTITE_DIALOG_PRIORITY_HIGH;
    msg->duration = dialog->default_duration;
    msg->elapsed = 0.0f;
    msg->event_id = -1;
    msg->metadata = 0;

    dialog->count++;

    /* Fire display callback for the new front message */
    if (dialog->display_callback) {
        dialog->text_elapsed = 0.0f;
        dialog->display_callback(dialog, msg, dialog->display_userdata);
    }

    return true;
}

/* ============================================================================
 * Event Registration
 * ========================================================================= */

bool agentite_dialog_register_event(Agentite_DialogSystem *dialog,
                                   int event_id,
                                   Agentite_SpeakerType speaker_type,
                                   const char *text) {
    return agentite_dialog_register_event_ex(dialog, event_id, speaker_type, 0, text,
                                            AGENTITE_DIALOG_PRIORITY_NORMAL, 0.0f, false);
}

bool agentite_dialog_register_event_ex(Agentite_DialogSystem *dialog,
                                      int event_id,
                                      Agentite_SpeakerType speaker_type,
                                      uint32_t speaker_id,
                                      const char *text,
                                      Agentite_DialogPriority priority,
                                      float duration,
                                      bool repeatable) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);
    AGENTITE_VALIDATE_PTR_RET(text, false);

    /* Check if event already exists */
    if (find_event(dialog, event_id) != NULL) {
        agentite_set_error("Dialog: Event ID %d already registered", event_id);
        return false;
    }

    if (dialog->event_count >= AGENTITE_DIALOG_MAX_EVENTS) {
        agentite_set_error("Dialog: Maximum events reached (%d)", AGENTITE_DIALOG_MAX_EVENTS);
        return false;
    }

    Agentite_DialogEvent *event = &dialog->events[dialog->event_count];
    event->id = event_id;
    strncpy(event->text, text, AGENTITE_DIALOG_MAX_TEXT - 1);
    event->text[AGENTITE_DIALOG_MAX_TEXT - 1] = '\0';
    event->speaker_type = speaker_type;
    event->speaker_id = speaker_id;
    event->priority = priority;
    event->duration = duration;
    event->repeatable = repeatable;
    event->active = true;

    dialog->event_states[dialog->event_count].triggered = false;
    dialog->event_count++;

    return true;
}

bool agentite_dialog_unregister_event(Agentite_DialogSystem *dialog, int event_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);

    int idx = find_event_index(dialog, event_id);
    if (idx < 0) {
        return false;
    }

    /* Mark as inactive (don't remove, just disable) */
    dialog->events[idx].active = false;
    return true;
}

bool agentite_dialog_trigger_event(Agentite_DialogSystem *dialog, int event_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);

    int idx = find_event_index(dialog, event_id);
    if (idx < 0) {
        return false;
    }

    Agentite_DialogEvent *event = &dialog->events[idx];
    Agentite_EventTriggerState *state = &dialog->event_states[idx];

    /* Check if already triggered (unless repeatable) */
    if (state->triggered && !event->repeatable) {
        return false;
    }

    /* Queue the message */
    bool queued = agentite_dialog_queue_message_ex(dialog,
                                                  event->speaker_type,
                                                  event->speaker_id,
                                                  event->text,
                                                  event->priority,
                                                  event->duration);

    if (queued) {
        /* Mark the newly queued message's event_id */
        int msg_idx = queue_prev(dialog, dialog->tail);
        dialog->messages[msg_idx].event_id = event_id;

        state->triggered = true;

        /* Fire event callback */
        if (dialog->event_callback) {
            dialog->event_callback(dialog, event_id, dialog->event_userdata);
        }
    }

    return queued;
}

bool agentite_dialog_event_triggered(Agentite_DialogSystem *dialog, int event_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);

    int idx = find_event_index(dialog, event_id);
    if (idx < 0) {
        return false;
    }

    return dialog->event_states[idx].triggered;
}

bool agentite_dialog_reset_event(Agentite_DialogSystem *dialog, int event_id) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);

    int idx = find_event_index(dialog, event_id);
    if (idx < 0) {
        return false;
    }

    dialog->event_states[idx].triggered = false;
    return true;
}

/* ============================================================================
 * Message Display
 * ========================================================================= */

bool agentite_dialog_has_message(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);
    return dialog->count > 0;
}

const Agentite_DialogMessage *agentite_dialog_current(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, NULL);

    if (dialog->count == 0) {
        return NULL;
    }

    return &dialog->messages[dialog->head];
}

void agentite_dialog_advance(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR(dialog);

    if (dialog->count == 0) {
        return;
    }

    /* Fire dismiss callback */
    if (dialog->dismiss_callback) {
        dialog->dismiss_callback(dialog, &dialog->messages[dialog->head],
                                  dialog->dismiss_userdata);
    }

    dialog->head = queue_next(dialog, dialog->head);
    dialog->count--;
    dialog->text_elapsed = 0.0f;

    /* Fire display callback for new current message */
    if (dialog->count > 0 && dialog->display_callback) {
        dialog->display_callback(dialog, &dialog->messages[dialog->head],
                                  dialog->display_userdata);
    }
}

bool agentite_dialog_update(Agentite_DialogSystem *dialog, float delta_time) {
    AGENTITE_VALIDATE_PTR_RET(dialog, false);

    if (dialog->count == 0) {
        return false;
    }

    Agentite_DialogMessage *msg = &dialog->messages[dialog->head];
    msg->elapsed += delta_time;
    dialog->text_elapsed += delta_time;

    /* Auto-advance if duration exceeded and animation complete */
    if (dialog->auto_advance &&
        msg->elapsed >= msg->duration &&
        agentite_dialog_animation_complete(dialog)) {
        agentite_dialog_advance(dialog);
        return true;
    }

    return false;
}

void agentite_dialog_skip_animation(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR(dialog);

    if (dialog->count == 0) {
        return;
    }

    /* Set text_elapsed to show all characters */
    Agentite_DialogMessage *msg = &dialog->messages[dialog->head];
    size_t len = strlen(msg->text);
    if (dialog->text_speed > 0.0f) {
        dialog->text_elapsed = (float)len / dialog->text_speed;
    }
}

bool agentite_dialog_animation_complete(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, true);

    if (dialog->count == 0 || dialog->text_speed <= 0.0f) {
        return true;  /* No message or instant text */
    }

    Agentite_DialogMessage *msg = &dialog->messages[dialog->head];
    size_t len = strlen(msg->text);
    int visible = (int)(dialog->text_elapsed * dialog->text_speed);

    return visible >= (int)len;
}

/* ============================================================================
 * Queue State
 * ========================================================================= */

int agentite_dialog_count(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 0);
    return dialog->count;
}

bool agentite_dialog_is_empty(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, true);
    return dialog->count == 0;
}

bool agentite_dialog_is_full(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, true);
    return dialog->count >= dialog->capacity;
}

const Agentite_DialogMessage *agentite_dialog_get(Agentite_DialogSystem *dialog, int index) {
    AGENTITE_VALIDATE_PTR_RET(dialog, NULL);

    if (index < 0 || index >= dialog->count) {
        return NULL;
    }

    int actual_index = (dialog->head + index) % dialog->capacity;
    return &dialog->messages[actual_index];
}

int agentite_dialog_capacity(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 0);
    return dialog->capacity;
}

/* ============================================================================
 * Configuration
 * ========================================================================= */

void agentite_dialog_set_default_duration(Agentite_DialogSystem *dialog, float duration) {
    AGENTITE_VALIDATE_PTR(dialog);
    dialog->default_duration = duration > 0.0f ? duration : 1.0f;
}

float agentite_dialog_get_default_duration(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 5.0f);
    return dialog->default_duration;
}

void agentite_dialog_set_text_speed(Agentite_DialogSystem *dialog, float chars_per_second) {
    AGENTITE_VALIDATE_PTR(dialog);
    dialog->text_speed = chars_per_second >= 0.0f ? chars_per_second : 0.0f;
}

float agentite_dialog_get_text_speed(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 0.0f);
    return dialog->text_speed;
}

int agentite_dialog_get_visible_chars(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, 0);

    if (dialog->count == 0) {
        return 0;
    }

    if (dialog->text_speed <= 0.0f) {
        return -1;  /* All visible (instant) */
    }

    Agentite_DialogMessage *msg = &dialog->messages[dialog->head];
    int visible = (int)(dialog->text_elapsed * dialog->text_speed);
    int len = (int)strlen(msg->text);

    if (visible >= len) {
        return -1;  /* All visible */
    }

    return visible;
}

void agentite_dialog_set_auto_advance(Agentite_DialogSystem *dialog, bool enabled) {
    AGENTITE_VALIDATE_PTR(dialog);
    dialog->auto_advance = enabled;
}

bool agentite_dialog_get_auto_advance(Agentite_DialogSystem *dialog) {
    AGENTITE_VALIDATE_PTR_RET(dialog, true);
    return dialog->auto_advance;
}

/* ============================================================================
 * Callbacks
 * ========================================================================= */

void agentite_dialog_set_display_callback(Agentite_DialogSystem *dialog,
                                         Agentite_DialogDisplayCallback callback,
                                         void *userdata) {
    AGENTITE_VALIDATE_PTR(dialog);
    dialog->display_callback = callback;
    dialog->display_userdata = userdata;
}

void agentite_dialog_set_dismiss_callback(Agentite_DialogSystem *dialog,
                                         Agentite_DialogDismissCallback callback,
                                         void *userdata) {
    AGENTITE_VALIDATE_PTR(dialog);
    dialog->dismiss_callback = callback;
    dialog->dismiss_userdata = userdata;
}

void agentite_dialog_set_event_callback(Agentite_DialogSystem *dialog,
                                       Agentite_DialogEventCallback callback,
                                       void *userdata) {
    AGENTITE_VALIDATE_PTR(dialog);
    dialog->event_callback = callback;
    dialog->event_userdata = userdata;
}

/* ============================================================================
 * Utility Functions
 * ========================================================================= */

const char *agentite_speaker_type_name(Agentite_SpeakerType type) {
    switch (type) {
        case AGENTITE_SPEAKER_SYSTEM:   return "System";
        case AGENTITE_SPEAKER_PLAYER:   return "Player";
        case AGENTITE_SPEAKER_AI:       return "AI";
        case AGENTITE_SPEAKER_NPC:      return "NPC";
        case AGENTITE_SPEAKER_ENEMY:    return "Enemy";
        case AGENTITE_SPEAKER_ALLY:     return "Ally";
        case AGENTITE_SPEAKER_TUTORIAL: return "Tutorial";
        default:
            if (type >= AGENTITE_SPEAKER_CUSTOM) {
                return "Custom";
            }
            return "Unknown";
    }
}

const char *agentite_dialog_priority_name(Agentite_DialogPriority priority) {
    switch (priority) {
        case AGENTITE_DIALOG_PRIORITY_LOW:      return "Low";
        case AGENTITE_DIALOG_PRIORITY_NORMAL:   return "Normal";
        case AGENTITE_DIALOG_PRIORITY_HIGH:     return "High";
        case AGENTITE_DIALOG_PRIORITY_CRITICAL: return "Critical";
        default:                              return "Unknown";
    }
}

uint32_t agentite_speaker_default_color(Agentite_SpeakerType type) {
    if (type >= 0 && type < AGENTITE_SPEAKER_COUNT) {
        return DEFAULT_SPEAKER_COLORS[type];
    }
    return 0xFFFFFFFF;
}
