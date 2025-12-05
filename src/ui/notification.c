#include "carbon/notification.h"
#include "carbon/text.h"
#include "carbon/validate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Notification manager structure */
struct Carbon_NotificationManager {
    Carbon_Notification notifications[CARBON_MAX_NOTIFICATIONS];
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

Carbon_NotificationManager *carbon_notify_create(void) {
    Carbon_NotificationManager *mgr = calloc(1, sizeof(Carbon_NotificationManager));
    if (!mgr) return NULL;

    mgr->count = 0;
    mgr->default_duration = CARBON_NOTIFICATION_DEFAULT_DURATION;
    mgr->newest_on_top = true;

    return mgr;
}

void carbon_notify_destroy(Carbon_NotificationManager *mgr) {
    free(mgr);
}

/* Internal: Add notification with full parameters */
static void add_notification(Carbon_NotificationManager *mgr,
                            const char *message,
                            float r, float g, float b, float a,
                            Carbon_NotifyType type,
                            float duration) {
    if (!mgr || !message) return;

    /* If queue is full, remove oldest notification */
    if (mgr->count >= CARBON_MAX_NOTIFICATIONS) {
        /* Shift all notifications down (remove index 0 = oldest) */
        memmove(&mgr->notifications[0], &mgr->notifications[1],
                sizeof(Carbon_Notification) * (CARBON_MAX_NOTIFICATIONS - 1));
        mgr->count = CARBON_MAX_NOTIFICATIONS - 1;
    }

    /* Add new notification at end (newest) */
    Carbon_Notification *notif = &mgr->notifications[mgr->count];
    strncpy(notif->message, message, CARBON_NOTIFICATION_MAX_LEN - 1);
    notif->message[CARBON_NOTIFICATION_MAX_LEN - 1] = '\0';
    notif->r = r;
    notif->g = g;
    notif->b = b;
    notif->a = a;
    notif->type = type;
    notif->time_remaining = duration;

    mgr->count++;
}

void carbon_notify_add(Carbon_NotificationManager *mgr, const char *message, Carbon_NotifyType type) {
    CARBON_VALIDATE_PTR(mgr);
    CARBON_VALIDATE_PTR(message);

    int t = (type < 0 || type > CARBON_NOTIFY_ERROR) ? CARBON_NOTIFY_INFO : type;
    add_notification(mgr, message,
                    type_colors[t][0], type_colors[t][1],
                    type_colors[t][2], type_colors[t][3],
                    type, mgr->default_duration);
}

void carbon_notify_add_timed(Carbon_NotificationManager *mgr, const char *message,
                              Carbon_NotifyType type, float duration) {
    CARBON_VALIDATE_PTR(mgr);
    CARBON_VALIDATE_PTR(message);

    int t = (type < 0 || type > CARBON_NOTIFY_ERROR) ? CARBON_NOTIFY_INFO : type;
    add_notification(mgr, message,
                    type_colors[t][0], type_colors[t][1],
                    type_colors[t][2], type_colors[t][3],
                    type, duration);
}

void carbon_notify_add_colored(Carbon_NotificationManager *mgr, const char *message,
                                float r, float g, float b) {
    CARBON_VALIDATE_PTR(mgr);
    CARBON_VALIDATE_PTR(message);

    add_notification(mgr, message, r, g, b, 1.0f,
                    CARBON_NOTIFY_INFO, mgr->default_duration);
}

void carbon_notify_printf(Carbon_NotificationManager *mgr, Carbon_NotifyType type,
                          const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    carbon_notify_printf_v(mgr, type, fmt, args);
    va_end(args);
}

void carbon_notify_printf_v(Carbon_NotificationManager *mgr, Carbon_NotifyType type,
                            const char *fmt, va_list args) {
    CARBON_VALIDATE_PTR(mgr);
    CARBON_VALIDATE_PTR(fmt);

    char buffer[CARBON_NOTIFICATION_MAX_LEN];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    carbon_notify_add(mgr, buffer, type);
}

void carbon_notify_update(Carbon_NotificationManager *mgr, float dt) {
    if (!mgr) return;

    int write = 0;
    for (int read = 0; read < mgr->count; read++) {
        Carbon_Notification *notif = &mgr->notifications[read];
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

void carbon_notify_clear(Carbon_NotificationManager *mgr) {
    if (!mgr) return;
    mgr->count = 0;
}

int carbon_notify_count(Carbon_NotificationManager *mgr) {
    return mgr ? mgr->count : 0;
}

const Carbon_Notification *carbon_notify_get(Carbon_NotificationManager *mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->count) return NULL;
    return &mgr->notifications[index];
}

void carbon_notify_set_default_duration(Carbon_NotificationManager *mgr, float duration) {
    if (!mgr) return;
    mgr->default_duration = duration > 0.0f ? duration : CARBON_NOTIFICATION_DEFAULT_DURATION;
}

float carbon_notify_get_default_duration(Carbon_NotificationManager *mgr) {
    return mgr ? mgr->default_duration : CARBON_NOTIFICATION_DEFAULT_DURATION;
}

void carbon_notify_set_newest_on_top(Carbon_NotificationManager *mgr, bool newest_on_top) {
    if (!mgr) return;
    mgr->newest_on_top = newest_on_top;
}

void carbon_notify_render(Carbon_NotificationManager *mgr,
                          Carbon_TextRenderer *text,
                          Carbon_Font *font,
                          float x, float y, float spacing) {
    if (!mgr || !text || !font || mgr->count == 0) return;

    if (mgr->newest_on_top) {
        /* Render newest first (at top), then older below */
        float curr_y = y;
        for (int i = mgr->count - 1; i >= 0; i--) {
            const Carbon_Notification *notif = &mgr->notifications[i];

            /* Fade out in last second */
            float alpha = notif->a;
            if (notif->time_remaining < 1.0f) {
                alpha *= notif->time_remaining;
            }

            carbon_text_draw_colored(text, font, notif->message, x, curr_y,
                                    notif->r, notif->g, notif->b, alpha);
            curr_y += spacing;
        }
    } else {
        /* Render oldest first (at top), newer below */
        float curr_y = y;
        for (int i = 0; i < mgr->count; i++) {
            const Carbon_Notification *notif = &mgr->notifications[i];

            float alpha = notif->a;
            if (notif->time_remaining < 1.0f) {
                alpha *= notif->time_remaining;
            }

            carbon_text_draw_colored(text, font, notif->message, x, curr_y,
                                    notif->r, notif->g, notif->b, alpha);
            curr_y += spacing;
        }
    }
}

void carbon_notify_get_type_color(Carbon_NotifyType type, float *r, float *g, float *b) {
    int t = (type < 0 || type > CARBON_NOTIFY_ERROR) ? CARBON_NOTIFY_INFO : type;
    if (r) *r = type_colors[t][0];
    if (g) *g = type_colors[t][1];
    if (b) *b = type_colors[t][2];
}
