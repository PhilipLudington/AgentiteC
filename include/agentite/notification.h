#ifndef AGENTITE_NOTIFICATION_H
#define AGENTITE_NOTIFICATION_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

/**
 * Carbon Notification/Toast System
 *
 * Timed notification messages with color coding for player feedback.
 * Messages auto-expire and are rendered as a stack on screen.
 *
 * Usage:
 *   Agentite_NotificationManager *notify = agentite_notify_create();
 *
 *   // Add notifications
 *   agentite_notify_add(notify, "Game saved!", AGENTITE_NOTIFY_SUCCESS);
 *   agentite_notify_add(notify, "Low resources!", AGENTITE_NOTIFY_WARNING);
 *   agentite_notify_printf(notify, AGENTITE_NOTIFY_INFO, "Score: %d", score);
 *
 *   // In game loop:
 *   agentite_notify_update(notify, delta_time);
 *
 *   // Render (during text batch)
 *   agentite_notify_render(notify, text_renderer, font, x, y, spacing);
 *
 *   agentite_notify_destroy(notify);
 */

/** Maximum number of simultaneous notifications */
#define AGENTITE_MAX_NOTIFICATIONS 8

/** Maximum message length */
#define AGENTITE_NOTIFICATION_MAX_LEN 128

/** Default duration in seconds */
#define AGENTITE_NOTIFICATION_DEFAULT_DURATION 5.0f

/**
 * Notification types with default colors
 */
typedef enum {
    AGENTITE_NOTIFY_INFO,      /**< White - general information */
    AGENTITE_NOTIFY_SUCCESS,   /**< Green - positive feedback */
    AGENTITE_NOTIFY_WARNING,   /**< Yellow/Orange - caution */
    AGENTITE_NOTIFY_ERROR      /**< Red - errors/failures */
} Agentite_NotifyType;

/**
 * Individual notification data
 */
typedef struct {
    char message[AGENTITE_NOTIFICATION_MAX_LEN]; /**< Message text */
    float time_remaining;                       /**< Seconds until expiration */
    float r, g, b, a;                          /**< Color (RGBA 0.0-1.0) */
    Agentite_NotifyType type;                    /**< Notification type */
} Agentite_Notification;

/**
 * Notification manager (opaque structure)
 */
typedef struct Agentite_NotificationManager Agentite_NotificationManager;

/**
 * Create a notification manager.
 *
 * @return New notification manager, or NULL on failure
 */
Agentite_NotificationManager *agentite_notify_create(void);

/**
 * Destroy a notification manager and free resources.
 *
 * @param mgr Notification manager to destroy
 */
void agentite_notify_destroy(Agentite_NotificationManager *mgr);

/**
 * Add a notification with default duration.
 *
 * @param mgr Notification manager
 * @param message Message text (truncated if too long)
 * @param type Notification type (determines color)
 */
void agentite_notify_add(Agentite_NotificationManager *mgr, const char *message, Agentite_NotifyType type);

/**
 * Add a notification with custom duration.
 *
 * @param mgr Notification manager
 * @param message Message text
 * @param type Notification type
 * @param duration Duration in seconds before auto-removal
 */
void agentite_notify_add_timed(Agentite_NotificationManager *mgr, const char *message,
                              Agentite_NotifyType type, float duration);

/**
 * Add a notification with custom color.
 *
 * @param mgr Notification manager
 * @param message Message text
 * @param r Red component (0.0-1.0)
 * @param g Green component (0.0-1.0)
 * @param b Blue component (0.0-1.0)
 */
void agentite_notify_add_colored(Agentite_NotificationManager *mgr, const char *message,
                                float r, float g, float b);

/**
 * Add a notification with printf-style formatting.
 *
 * @param mgr Notification manager
 * @param type Notification type
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void agentite_notify_printf(Agentite_NotificationManager *mgr, Agentite_NotifyType type,
                          const char *fmt, ...);

/**
 * Add a notification with printf-style formatting (va_list version).
 *
 * @param mgr Notification manager
 * @param type Notification type
 * @param fmt Printf-style format string
 * @param args Format arguments
 */
void agentite_notify_printf_v(Agentite_NotificationManager *mgr, Agentite_NotifyType type,
                            const char *fmt, va_list args);

/**
 * Update all notifications (call each frame).
 * Removes expired notifications.
 *
 * @param mgr Notification manager
 * @param dt Delta time in seconds
 */
void agentite_notify_update(Agentite_NotificationManager *mgr, float dt);

/**
 * Clear all notifications.
 *
 * @param mgr Notification manager
 */
void agentite_notify_clear(Agentite_NotificationManager *mgr);

/**
 * Get the number of active notifications.
 *
 * @param mgr Notification manager
 * @return Number of notifications currently displayed
 */
int agentite_notify_count(Agentite_NotificationManager *mgr);

/**
 * Get a notification by index.
 *
 * @param mgr Notification manager
 * @param index Notification index (0 = oldest, count-1 = newest)
 * @return Pointer to notification data, or NULL if invalid index
 */
const Agentite_Notification *agentite_notify_get(Agentite_NotificationManager *mgr, int index);

/**
 * Set the default notification duration.
 *
 * @param mgr Notification manager
 * @param duration Duration in seconds
 */
void agentite_notify_set_default_duration(Agentite_NotificationManager *mgr, float duration);

/**
 * Get the default notification duration.
 *
 * @param mgr Notification manager
 * @return Default duration in seconds
 */
float agentite_notify_get_default_duration(Agentite_NotificationManager *mgr);

/**
 * Set whether newer notifications appear at top (default) or bottom.
 *
 * @param mgr Notification manager
 * @param newest_on_top true = newest at top, false = newest at bottom
 */
void agentite_notify_set_newest_on_top(Agentite_NotificationManager *mgr, bool newest_on_top);

/*============================================================================
 * Rendering Integration
 *============================================================================*/

/* Forward declarations for optional rendering integration */
struct Agentite_TextRenderer;
struct Agentite_Font;

/**
 * Render notifications using the Carbon text renderer.
 * Call this during your text batch (between agentite_text_begin/end).
 *
 * @param mgr Notification manager
 * @param text Text renderer
 * @param font Font to use
 * @param x X position of notification stack
 * @param y Y position of notification stack (top if newest_on_top, bottom otherwise)
 * @param spacing Vertical spacing between notifications
 */
void agentite_notify_render(Agentite_NotificationManager *mgr,
                          struct Agentite_TextRenderer *text,
                          struct Agentite_Font *font,
                          float x, float y, float spacing);

/**
 * Get the color for a notification type.
 * Useful for custom rendering.
 *
 * @param type Notification type
 * @param r Output red component
 * @param g Output green component
 * @param b Output blue component
 */
void agentite_notify_get_type_color(Agentite_NotifyType type, float *r, float *g, float *b);

#endif /* AGENTITE_NOTIFICATION_H */
