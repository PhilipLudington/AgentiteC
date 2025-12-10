#include "agentite/agentite.h"
#include "agentite/notification.h"
#include "agentite/text.h"
#include "agentite/validate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Notification manager structure */
struct Agentite_NotificationManager {
    Agentite_Notification notifications[AGENTITE_MAX_NOTIFICATIONS];
    int count;
    float default_duration;
    bool newest_on_top;
};

/* Default colors for notification types (RGBA) */
static const float type_colors[][4] = {
    { 1.0f, 1.0f, 1.0f, 1.0f },  /* INFO - white */
    { 0.3f, 0.9f, 0.3f, 1.0f },  /* SUCCESS - green */
    { 1.0f, 0.8f, 0.2f, 1.0f },  /* WARNING - yellow/orange */
    { 1.0f, 0.3f, 0.3f, 1.0f }   /* ERROR - red */
};

Agentite_NotificationManager *agentite_notify_create(void) {
    Agentite_NotificationManager *mgr = AGENTITE_ALLOC(Agentite_NotificationManager);
    if (!mgr) return NULL;

    mgr->count = 0;
    mgr->default_duration = AGENTITE_NOTIFICATION_DEFAULT_DURATION;
    mgr->newest_on_top = true;

    return mgr;
}

void agentite_notify_destroy(Agentite_NotificationManager *mgr) {
    free(mgr);
}

/* Internal: Add notification with full parameters */
static void add_notification(Agentite_NotificationManager *mgr,
                            const char *message,
                            float r, float g, float b, float a,
                            Agentite_NotifyType type,
                            float duration) {
    if (!mgr || !message) return;

    /* If queue is full, remove oldest notification */
    if (mgr->count >= AGENTITE_MAX_NOTIFICATIONS) {
        /* Shift all notifications down (remove index 0 = oldest) */
        memmove(&mgr->notifications[0], &mgr->notifications[1],
                sizeof(Agentite_Notification) * (AGENTITE_MAX_NOTIFICATIONS - 1));
        mgr->count = AGENTITE_MAX_NOTIFICATIONS - 1;
    }

    /* Add new notification at end (newest) */
    Agentite_Notification *notif = &mgr->notifications[mgr->count];
    strncpy(notif->message, message, AGENTITE_NOTIFICATION_MAX_LEN - 1);
    notif->message[AGENTITE_NOTIFICATION_MAX_LEN - 1] = '\0';
    notif->r = r;
    notif->g = g;
    notif->b = b;
    notif->a = a;
    notif->type = type;
    notif->time_remaining = duration;

    mgr->count++;
}

void agentite_notify_add(Agentite_NotificationManager *mgr, const char *message, Agentite_NotifyType type) {
    AGENTITE_VALIDATE_PTR(mgr);
    AGENTITE_VALIDATE_PTR(message);

    int t = (type < 0 || type > AGENTITE_NOTIFY_ERROR) ? AGENTITE_NOTIFY_INFO : type;
    add_notification(mgr, message,
                    type_colors[t][0], type_colors[t][1],
                    type_colors[t][2], type_colors[t][3],
                    type, mgr->default_duration);
}

void agentite_notify_add_timed(Agentite_NotificationManager *mgr, const char *message,
                              Agentite_NotifyType type, float duration) {
    AGENTITE_VALIDATE_PTR(mgr);
    AGENTITE_VALIDATE_PTR(message);

    int t = (type < 0 || type > AGENTITE_NOTIFY_ERROR) ? AGENTITE_NOTIFY_INFO : type;
    add_notification(mgr, message,
                    type_colors[t][0], type_colors[t][1],
                    type_colors[t][2], type_colors[t][3],
                    type, duration);
}

void agentite_notify_add_colored(Agentite_NotificationManager *mgr, const char *message,
                                float r, float g, float b) {
    AGENTITE_VALIDATE_PTR(mgr);
    AGENTITE_VALIDATE_PTR(message);

    add_notification(mgr, message, r, g, b, 1.0f,
                    AGENTITE_NOTIFY_INFO, mgr->default_duration);
}

void agentite_notify_printf(Agentite_NotificationManager *mgr, Agentite_NotifyType type,
                          const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    agentite_notify_printf_v(mgr, type, fmt, args);
    va_end(args);
}

void agentite_notify_printf_v(Agentite_NotificationManager *mgr, Agentite_NotifyType type,
                            const char *fmt, va_list args) {
    AGENTITE_VALIDATE_PTR(mgr);
    AGENTITE_VALIDATE_PTR(fmt);

    char buffer[AGENTITE_NOTIFICATION_MAX_LEN];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    agentite_notify_add(mgr, buffer, type);
}

void agentite_notify_update(Agentite_NotificationManager *mgr, float dt) {
    if (!mgr) return;

    int write = 0;
    for (int read = 0; read < mgr->count; read++) {
        Agentite_Notification *notif = &mgr->notifications[read];
        notif->time_remaining -= dt;

        if (notif->time_remaining > 0.0f) {
            /* Keep this notification */
            if (write != read) {
                mgr->notifications[write] = *notif;
            }
            write++;
        }
        /* Else: notification expired, skip it */
    }
    mgr->count = write;
}

void agentite_notify_clear(Agentite_NotificationManager *mgr) {
    if (!mgr) return;
    mgr->count = 0;
}

int agentite_notify_count(Agentite_NotificationManager *mgr) {
    return mgr ? mgr->count : 0;
}

const Agentite_Notification *agentite_notify_get(Agentite_NotificationManager *mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->count) return NULL;
    return &mgr->notifications[index];
}

void agentite_notify_set_default_duration(Agentite_NotificationManager *mgr, float duration) {
    if (!mgr) return;
    mgr->default_duration = duration > 0.0f ? duration : AGENTITE_NOTIFICATION_DEFAULT_DURATION;
}

float agentite_notify_get_default_duration(Agentite_NotificationManager *mgr) {
    return mgr ? mgr->default_duration : AGENTITE_NOTIFICATION_DEFAULT_DURATION;
}

void agentite_notify_set_newest_on_top(Agentite_NotificationManager *mgr, bool newest_on_top) {
    if (!mgr) return;
    mgr->newest_on_top = newest_on_top;
}

void agentite_notify_render(Agentite_NotificationManager *mgr,
                          Agentite_TextRenderer *text,
                          Agentite_Font *font,
                          float x, float y, float spacing) {
    if (!mgr || !text || !font || mgr->count == 0) return;

    if (mgr->newest_on_top) {
        /* Render newest first (at top), then older below */
        float curr_y = y;
        for (int i = mgr->count - 1; i >= 0; i--) {
            const Agentite_Notification *notif = &mgr->notifications[i];

            /* Fade out in last second */
            float alpha = notif->a;
            if (notif->time_remaining < 1.0f) {
                alpha *= notif->time_remaining;
            }

            agentite_text_draw_colored(text, font, notif->message, x, curr_y,
                                    notif->r, notif->g, notif->b, alpha);
            curr_y += spacing;
        }
    } else {
        /* Render oldest first (at top), newer below */
        float curr_y = y;
        for (int i = 0; i < mgr->count; i++) {
            const Agentite_Notification *notif = &mgr->notifications[i];

            float alpha = notif->a;
            if (notif->time_remaining < 1.0f) {
                alpha *= notif->time_remaining;
            }

            agentite_text_draw_colored(text, font, notif->message, x, curr_y,
                                    notif->r, notif->g, notif->b, alpha);
            curr_y += spacing;
        }
    }
}

void agentite_notify_get_type_color(Agentite_NotifyType type, float *r, float *g, float *b) {
    int t = (type < 0 || type > AGENTITE_NOTIFY_ERROR) ? AGENTITE_NOTIFY_INFO : type;
    if (r) *r = type_colors[t][0];
    if (g) *g = type_colors[t][1];
    if (b) *b = type_colors[t][2];
}
