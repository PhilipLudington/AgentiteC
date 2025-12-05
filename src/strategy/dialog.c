/**
 * @file dialog.c
 * @brief Dialog / Narrative System implementation
 */

#include "carbon/dialog.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Triggered event tracking
 */
typedef struct Carbon_EventTriggerState {
    bool triggered;     /**< Has this event been triggered */
} Carbon_EventTriggerState;

/**
 * @brief Dialog system structure
 */
struct Carbon_DialogSystem {
    /* Message queue */
    Carbon_DialogMessage *messages;     /**< Message queue (circular buffer) */
    int capacity;                       /**< Queue capacity */
    int count;                          /**< Current message count */
    int head;                           /**< Queue head index */
    int tail;                           /**< Queue tail index */

    /* Custom speakers */
    Carbon_Speaker speakers[CARBON_DIALOG_MAX_SPEAKERS];
    int speaker_count;
    uint32_t next_speaker_id;

    /* Built-in speaker customization */
    char builtin_names[CARBON_SPEAKER_COUNT][CARBON_DIALOG_MAX_SPEAKER_NAME];
    uint32_t builtin_colors[CARBON_SPEAKER_COUNT];

    /* Event definitions */
    Carbon_DialogEvent events[CARBON_DIALOG_MAX_EVENTS];
    Carbon_EventTriggerState event_states[CARBON_DIALOG_MAX_EVENTS];
    int event_count;

    /* Configuration */
    float default_duration;             /**< Default message duration */
    float text_speed;                   /**< Characters per second (0 = instant) */
    float text_elapsed;                 /**< Time since current message started */
    bool auto_advance;                  /**< Auto-advance after duration */

    /* Callbacks */
    Carbon_DialogDisplayCallback display_callback;
    void *display_userdata;
    Carbon_DialogDismissCallback dismiss_callback;
    void *dismiss_userdata;
    Carbon_DialogEventCallback event_callback;
    void *event_userdata;
};

/* ============================================================================
 * Default Colors
 * ========================================================================= */

static const uint32_t DEFAULT_SPEAKER_COLORS[CARBON_SPEAKER_COUNT] = {
    0xFFCCCCCC,  /* SYSTEM - light gray */
    0xFF00FF00,  /* PLAYER - green */
    0xFF00CCFF,  /* AI - cyan */
    0xFFFFFFFF,  /* NPC - white */
    0xFF0000FF,  /* ENEMY - red */
    0xFF00FF80,  /* ALLY - light green */
    0xFFFFFF00,  /* TUTORIAL - yellow */
};

static const char *DEFAULT_SPEAKER_NAMES[CARBON_SPEAKER_COUNT] = {
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

static inline int queue_next(Carbon_DialogSystem *dialog, int index) {
    return (index + 1) % dialog->capacity;
}

static inline int queue_prev(Carbon_DialogSystem *dialog, int index) {
    return (index - 1 + dialog->capacity) % dialog->capacity;
}

static Carbon_DialogEvent *find_event(Carbon_DialogSystem *dialog, int event_id) {
    for (int i = 0; i < dialog->event_count; i++) {
        if (dialog->events[i].active && dialog->events[i].id == event_id) {
            return &dialog->events[i];
        }
    }
    return NULL;
}

static int find_event_index(Carbon_DialogSystem *dialog, int event_id) {
    for (int i = 0; i < dialog->event_count; i++) {
        if (dialog->events[i].active && dialog->events[i].id == event_id) {
            return i;
        }
    }
    return -1;
}

static const Carbon_Speaker *find_speaker(Carbon_DialogSystem *dialog, uint32_t speaker_id) {
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

Carbon_DialogSystem *carbon_dialog_create(int max_messages) {
    if (max_messages <= 0) {
        carbon_set_error("Dialog: Invalid queue capacity %d", max_messages);
        return NULL;
    }

    Carbon_DialogSystem *dialog = calloc(1, sizeof(Carbon_DialogSystem));
    if (!dialog) {
        carbon_set_error("Dialog: Failed to allocate dialog system");
        return NULL;
    }

    dialog->messages = calloc(max_messages, sizeof(Carbon_DialogMessage));
    if (!dialog->messages) {
        carbon_set_error("Dialog: Failed to allocate message queue");
        free(dialog);
        return NULL;
    }

    dialog->capacity = max_messages;
    dialog->count = 0;
    dialog->head = 0;
    dialog->tail = 0;

    dialog->speaker_count = 0;
    dialog->next_speaker_id = CARBON_SPEAKER_CUSTOM;

    /* Initialize built-in speaker names and colors */
    for (int i = 0; i < CARBON_SPEAKER_COUNT; i++) {
        strncpy(dialog->builtin_names[i], DEFAULT_SPEAKER_NAMES[i],
                CARBON_DIALOG_MAX_SPEAKER_NAME - 1);
        dialog->builtin_colors[i] = DEFAULT_SPEAKER_COLORS[i];
    }

    dialog->event_count = 0;

    dialog->default_duration = 5.0f;
    dialog->text_speed = 0.0f;  /* Instant by default */
    dialog->text_elapsed = 0.0f;
    dialog->auto_advance = true;

    return dialog;
}

void carbon_dialog_destroy(Carbon_DialogSystem *dialog) {
    if (!dialog) return;
    free(dialog->messages);
    free(dialog);
}

void carbon_dialog_clear(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR(dialog);

    dialog->count = 0;
    dialog->head = 0;
    dialog->tail = 0;
    dialog->text_elapsed = 0.0f;
}

void carbon_dialog_reset_events(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR(dialog);

    for (int i = 0; i < CARBON_DIALOG_MAX_EVENTS; i++) {
        dialog->event_states[i].triggered = false;
    }
}

/* ============================================================================
 * Speaker Management
 * ========================================================================= */

uint32_t carbon_dialog_register_speaker(Carbon_DialogSystem *dialog,
                                         const char *name,
                                         uint32_t color,
                                         int portrait_id) {
    CARBON_VALIDATE_PTR_RET(dialog, 0);
    CARBON_VALIDATE_PTR_RET(name, 0);

    Carbon_Speaker speaker = {0};
    speaker.type = CARBON_SPEAKER_CUSTOM;
    speaker.color = color;
    speaker.portrait_id = portrait_id;
    strncpy(speaker.name, name, CARBON_DIALOG_MAX_SPEAKER_NAME - 1);

    return carbon_dialog_register_speaker_ex(dialog, &speaker);
}

uint32_t carbon_dialog_register_speaker_ex(Carbon_DialogSystem *dialog,
                                            const Carbon_Speaker *speaker) {
    CARBON_VALIDATE_PTR_RET(dialog, 0);
    CARBON_VALIDATE_PTR_RET(speaker, 0);

    if (dialog->speaker_count >= CARBON_DIALOG_MAX_SPEAKERS) {
        carbon_set_error("Dialog: Maximum speakers reached (%d)", CARBON_DIALOG_MAX_SPEAKERS);
        return 0;
    }

    uint32_t id = dialog->next_speaker_id++;
    Carbon_Speaker *s = &dialog->speakers[dialog->speaker_count++];
    *s = *speaker;
    s->id = id;

    return id;
}

const Carbon_Speaker *carbon_dialog_get_speaker(Carbon_DialogSystem *dialog,
                                                 uint32_t speaker_id) {
    CARBON_VALIDATE_PTR_RET(dialog, NULL);
    return find_speaker(dialog, speaker_id);
}

const char *carbon_dialog_get_speaker_name(Carbon_DialogSystem *dialog,
                                            Carbon_SpeakerType speaker_type,
                                            uint32_t speaker_id) {
    CARBON_VALIDATE_PTR_RET(dialog, "Unknown");

    if (speaker_type >= CARBON_SPEAKER_CUSTOM) {
        const Carbon_Speaker *speaker = find_speaker(dialog, speaker_id);
        return speaker ? speaker->name : "Unknown";
    }

    if (speaker_type >= 0 && speaker_type < CARBON_SPEAKER_COUNT) {
        return dialog->builtin_names[speaker_type];
    }

    return "Unknown";
}

uint32_t carbon_dialog_get_speaker_color(Carbon_DialogSystem *dialog,
                                          Carbon_SpeakerType speaker_type,
                                          uint32_t speaker_id) {
    CARBON_VALIDATE_PTR_RET(dialog, 0xFFFFFFFF);

    if (speaker_type >= CARBON_SPEAKER_CUSTOM) {
        const Carbon_Speaker *speaker = find_speaker(dialog, speaker_id);
        return speaker ? speaker->color : 0xFFFFFFFF;
    }

    if (speaker_type >= 0 && speaker_type < CARBON_SPEAKER_COUNT) {
        return dialog->builtin_colors[speaker_type];
    }

    return 0xFFFFFFFF;
}

void carbon_dialog_set_speaker_name(Carbon_DialogSystem *dialog,
                                     Carbon_SpeakerType type,
                                     const char *name) {
    CARBON_VALIDATE_PTR(dialog);
    CARBON_VALIDATE_PTR(name);

    if (type >= 0 && type < CARBON_SPEAKER_COUNT) {
        strncpy(dialog->builtin_names[type], name, CARBON_DIALOG_MAX_SPEAKER_NAME - 1);
        dialog->builtin_names[type][CARBON_DIALOG_MAX_SPEAKER_NAME - 1] = '\0';
    }
}

void carbon_dialog_set_speaker_color(Carbon_DialogSystem *dialog,
                                      Carbon_SpeakerType type,
                                      uint32_t color) {
    CARBON_VALIDATE_PTR(dialog);

    if (type >= 0 && type < CARBON_SPEAKER_COUNT) {
        dialog->builtin_colors[type] = color;
    }
}

/* ============================================================================
 * Message Queuing
 * ========================================================================= */

bool carbon_dialog_queue_message(Carbon_DialogSystem *dialog,
                                  Carbon_SpeakerType speaker_type,
                                  const char *text) {
    return carbon_dialog_queue_message_ex(dialog, speaker_type, 0, text,
                                           CARBON_DIALOG_PRIORITY_NORMAL, 0.0f);
}

bool carbon_dialog_queue_message_custom(Carbon_DialogSystem *dialog,
                                         uint32_t speaker_id,
                                         const char *text) {
    return carbon_dialog_queue_message_ex(dialog, CARBON_SPEAKER_CUSTOM, speaker_id, text,
                                           CARBON_DIALOG_PRIORITY_NORMAL, 0.0f);
}

bool carbon_dialog_queue_message_ex(Carbon_DialogSystem *dialog,
                                     Carbon_SpeakerType speaker_type,
                                     uint32_t speaker_id,
                                     const char *text,
                                     Carbon_DialogPriority priority,
                                     float duration) {
    CARBON_VALIDATE_PTR_RET(dialog, false);
    CARBON_VALIDATE_PTR_RET(text, false);

    if (dialog->count >= dialog->capacity) {
        carbon_set_error("Dialog: Message queue full");
        return false;
    }

    Carbon_DialogMessage *msg = &dialog->messages[dialog->tail];
    memset(msg, 0, sizeof(Carbon_DialogMessage));

    strncpy(msg->text, text, CARBON_DIALOG_MAX_TEXT - 1);
    msg->text[CARBON_DIALOG_MAX_TEXT - 1] = '\0';

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

bool carbon_dialog_queue_printf(Carbon_DialogSystem *dialog,
                                 Carbon_SpeakerType speaker_type,
                                 const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    bool result = carbon_dialog_queue_vprintf(dialog, speaker_type, fmt, args);
    va_end(args);
    return result;
}

bool carbon_dialog_queue_vprintf(Carbon_DialogSystem *dialog,
                                  Carbon_SpeakerType speaker_type,
                                  const char *fmt, va_list args) {
    CARBON_VALIDATE_PTR_RET(dialog, false);
    CARBON_VALIDATE_PTR_RET(fmt, false);

    char buffer[CARBON_DIALOG_MAX_TEXT];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    return carbon_dialog_queue_message(dialog, speaker_type, buffer);
}

bool carbon_dialog_insert_front(Carbon_DialogSystem *dialog,
                                 Carbon_SpeakerType speaker_type,
                                 const char *text) {
    CARBON_VALIDATE_PTR_RET(dialog, false);
    CARBON_VALIDATE_PTR_RET(text, false);

    if (dialog->count >= dialog->capacity) {
        carbon_set_error("Dialog: Message queue full");
        return false;
    }

    /* Insert at head (before current message) */
    dialog->head = queue_prev(dialog, dialog->head);

    Carbon_DialogMessage *msg = &dialog->messages[dialog->head];
    memset(msg, 0, sizeof(Carbon_DialogMessage));

    strncpy(msg->text, text, CARBON_DIALOG_MAX_TEXT - 1);
    msg->text[CARBON_DIALOG_MAX_TEXT - 1] = '\0';

    msg->speaker_type = speaker_type;
    msg->speaker_id = 0;
    msg->priority = CARBON_DIALOG_PRIORITY_HIGH;
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

bool carbon_dialog_register_event(Carbon_DialogSystem *dialog,
                                   int event_id,
                                   Carbon_SpeakerType speaker_type,
                                   const char *text) {
    return carbon_dialog_register_event_ex(dialog, event_id, speaker_type, 0, text,
                                            CARBON_DIALOG_PRIORITY_NORMAL, 0.0f, false);
}

bool carbon_dialog_register_event_ex(Carbon_DialogSystem *dialog,
                                      int event_id,
                                      Carbon_SpeakerType speaker_type,
                                      uint32_t speaker_id,
                                      const char *text,
                                      Carbon_DialogPriority priority,
                                      float duration,
                                      bool repeatable) {
    CARBON_VALIDATE_PTR_RET(dialog, false);
    CARBON_VALIDATE_PTR_RET(text, false);

    /* Check if event already exists */
    if (find_event(dialog, event_id) != NULL) {
        carbon_set_error("Dialog: Event ID %d already registered", event_id);
        return false;
    }

    if (dialog->event_count >= CARBON_DIALOG_MAX_EVENTS) {
        carbon_set_error("Dialog: Maximum events reached (%d)", CARBON_DIALOG_MAX_EVENTS);
        return false;
    }

    Carbon_DialogEvent *event = &dialog->events[dialog->event_count];
    event->id = event_id;
    strncpy(event->text, text, CARBON_DIALOG_MAX_TEXT - 1);
    event->text[CARBON_DIALOG_MAX_TEXT - 1] = '\0';
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

bool carbon_dialog_unregister_event(Carbon_DialogSystem *dialog, int event_id) {
    CARBON_VALIDATE_PTR_RET(dialog, false);

    int idx = find_event_index(dialog, event_id);
    if (idx < 0) {
        return false;
    }

    /* Mark as inactive (don't remove, just disable) */
    dialog->events[idx].active = false;
    return true;
}

bool carbon_dialog_trigger_event(Carbon_DialogSystem *dialog, int event_id) {
    CARBON_VALIDATE_PTR_RET(dialog, false);

    int idx = find_event_index(dialog, event_id);
    if (idx < 0) {
        return false;
    }

    Carbon_DialogEvent *event = &dialog->events[idx];
    Carbon_EventTriggerState *state = &dialog->event_states[idx];

    /* Check if already triggered (unless repeatable) */
    if (state->triggered && !event->repeatable) {
        return false;
    }

    /* Queue the message */
    bool queued = carbon_dialog_queue_message_ex(dialog,
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

bool carbon_dialog_event_triggered(Carbon_DialogSystem *dialog, int event_id) {
    CARBON_VALIDATE_PTR_RET(dialog, false);

    int idx = find_event_index(dialog, event_id);
    if (idx < 0) {
        return false;
    }

    return dialog->event_states[idx].triggered;
}

bool carbon_dialog_reset_event(Carbon_DialogSystem *dialog, int event_id) {
    CARBON_VALIDATE_PTR_RET(dialog, false);

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

bool carbon_dialog_has_message(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, false);
    return dialog->count > 0;
}

const Carbon_DialogMessage *carbon_dialog_current(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, NULL);

    if (dialog->count == 0) {
        return NULL;
    }

    return &dialog->messages[dialog->head];
}

void carbon_dialog_advance(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR(dialog);

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

bool carbon_dialog_update(Carbon_DialogSystem *dialog, float delta_time) {
    CARBON_VALIDATE_PTR_RET(dialog, false);

    if (dialog->count == 0) {
        return false;
    }

    Carbon_DialogMessage *msg = &dialog->messages[dialog->head];
    msg->elapsed += delta_time;
    dialog->text_elapsed += delta_time;

    /* Auto-advance if duration exceeded and animation complete */
    if (dialog->auto_advance &&
        msg->elapsed >= msg->duration &&
        carbon_dialog_animation_complete(dialog)) {
        carbon_dialog_advance(dialog);
        return true;
    }

    return false;
}

void carbon_dialog_skip_animation(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR(dialog);

    if (dialog->count == 0) {
        return;
    }

    /* Set text_elapsed to show all characters */
    Carbon_DialogMessage *msg = &dialog->messages[dialog->head];
    size_t len = strlen(msg->text);
    if (dialog->text_speed > 0.0f) {
        dialog->text_elapsed = (float)len / dialog->text_speed;
    }
}

bool carbon_dialog_animation_complete(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, true);

    if (dialog->count == 0 || dialog->text_speed <= 0.0f) {
        return true;  /* No message or instant text */
    }

    Carbon_DialogMessage *msg = &dialog->messages[dialog->head];
    size_t len = strlen(msg->text);
    int visible = (int)(dialog->text_elapsed * dialog->text_speed);

    return visible >= (int)len;
}

/* ============================================================================
 * Queue State
 * ========================================================================= */

int carbon_dialog_count(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, 0);
    return dialog->count;
}

bool carbon_dialog_is_empty(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, true);
    return dialog->count == 0;
}

bool carbon_dialog_is_full(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, true);
    return dialog->count >= dialog->capacity;
}

const Carbon_DialogMessage *carbon_dialog_get(Carbon_DialogSystem *dialog, int index) {
    CARBON_VALIDATE_PTR_RET(dialog, NULL);

    if (index < 0 || index >= dialog->count) {
        return NULL;
    }

    int actual_index = (dialog->head + index) % dialog->capacity;
    return &dialog->messages[actual_index];
}

int carbon_dialog_capacity(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, 0);
    return dialog->capacity;
}

/* ============================================================================
 * Configuration
 * ========================================================================= */

void carbon_dialog_set_default_duration(Carbon_DialogSystem *dialog, float duration) {
    CARBON_VALIDATE_PTR(dialog);
    dialog->default_duration = duration > 0.0f ? duration : 1.0f;
}

float carbon_dialog_get_default_duration(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, 5.0f);
    return dialog->default_duration;
}

void carbon_dialog_set_text_speed(Carbon_DialogSystem *dialog, float chars_per_second) {
    CARBON_VALIDATE_PTR(dialog);
    dialog->text_speed = chars_per_second >= 0.0f ? chars_per_second : 0.0f;
}

float carbon_dialog_get_text_speed(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, 0.0f);
    return dialog->text_speed;
}

int carbon_dialog_get_visible_chars(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, 0);

    if (dialog->count == 0) {
        return 0;
    }

    if (dialog->text_speed <= 0.0f) {
        return -1;  /* All visible (instant) */
    }

    Carbon_DialogMessage *msg = &dialog->messages[dialog->head];
    int visible = (int)(dialog->text_elapsed * dialog->text_speed);
    int len = (int)strlen(msg->text);

    if (visible >= len) {
        return -1;  /* All visible */
    }

    return visible;
}

void carbon_dialog_set_auto_advance(Carbon_DialogSystem *dialog, bool enabled) {
    CARBON_VALIDATE_PTR(dialog);
    dialog->auto_advance = enabled;
}

bool carbon_dialog_get_auto_advance(Carbon_DialogSystem *dialog) {
    CARBON_VALIDATE_PTR_RET(dialog, true);
    return dialog->auto_advance;
}

/* ============================================================================
 * Callbacks
 * ========================================================================= */

void carbon_dialog_set_display_callback(Carbon_DialogSystem *dialog,
                                         Carbon_DialogDisplayCallback callback,
                                         void *userdata) {
    CARBON_VALIDATE_PTR(dialog);
    dialog->display_callback = callback;
    dialog->display_userdata = userdata;
}

void carbon_dialog_set_dismiss_callback(Carbon_DialogSystem *dialog,
                                         Carbon_DialogDismissCallback callback,
                                         void *userdata) {
    CARBON_VALIDATE_PTR(dialog);
    dialog->dismiss_callback = callback;
    dialog->dismiss_userdata = userdata;
}

void carbon_dialog_set_event_callback(Carbon_DialogSystem *dialog,
                                       Carbon_DialogEventCallback callback,
                                       void *userdata) {
    CARBON_VALIDATE_PTR(dialog);
    dialog->event_callback = callback;
    dialog->event_userdata = userdata;
}

/* ============================================================================
 * Utility Functions
 * ========================================================================= */

const char *carbon_speaker_type_name(Carbon_SpeakerType type) {
    switch (type) {
        case CARBON_SPEAKER_SYSTEM:   return "System";
        case CARBON_SPEAKER_PLAYER:   return "Player";
        case CARBON_SPEAKER_AI:       return "AI";
        case CARBON_SPEAKER_NPC:      return "NPC";
        case CARBON_SPEAKER_ENEMY:    return "Enemy";
        case CARBON_SPEAKER_ALLY:     return "Ally";
        case CARBON_SPEAKER_TUTORIAL: return "Tutorial";
        default:
            if (type >= CARBON_SPEAKER_CUSTOM) {
                return "Custom";
            }
            return "Unknown";
    }
}

const char *carbon_dialog_priority_name(Carbon_DialogPriority priority) {
    switch (priority) {
        case CARBON_DIALOG_PRIORITY_LOW:      return "Low";
        case CARBON_DIALOG_PRIORITY_NORMAL:   return "Normal";
        case CARBON_DIALOG_PRIORITY_HIGH:     return "High";
        case CARBON_DIALOG_PRIORITY_CRITICAL: return "Critical";
        default:                              return "Unknown";
    }
}

uint32_t carbon_speaker_default_color(Carbon_SpeakerType type) {
    if (type >= 0 && type < CARBON_SPEAKER_COUNT) {
        return DEFAULT_SPEAKER_COLORS[type];
    }
    return 0xFFFFFFFF;
}
