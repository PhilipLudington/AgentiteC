#ifndef CARBON_NOTIFICATION_H
#define CARBON_NOTIFICATION_H

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
 *   Carbon_NotificationManager *notify = carbon_notify_create();
 *
 *   // Add notifications
 *   carbon_notify_add(notify, "Game saved!", CARBON_NOTIFY_SUCCESS);
 *   carbon_notify_add(notify, "Low resources!", CARBON_NOTIFY_WARNING);
 *   carbon_notify_printf(notify, CARBON_NOTIFY_INFO, "Score: %d", score);
 *
 *   // In game loop:
 *   carbon_notify_update(notify, delta_time);
 *
 *   // Render (during text batch)
 *   carbon_notify_render(notify, text_renderer, font, x, y, spacing);
 *
 *   carbon_notify_destroy(notify);
 */

/** Maximum number of simultaneous notifications */
#define CARBON_MAX_NOTIFICATIONS 8

/** Maximum message length */
#define CARBON_NOTIFICATION_MAX_LEN 128

/** Default duration in seconds */
#define CARBON_NOTIFICATION_DEFAULT_DURATION 5.0f

/**
 * Notification types with default colors
 */
typedef enum {
    CARBON_NOTIFY_INFO,      /**< White - general information */
    CARBON_NOTIFY_SUCCESS,   /**< Green - positive feedback */
    CARBON_NOTIFY_WARNING,   /**< Yellow/Orange - caution */
    CARBON_NOTIFY_ERROR      /**< Red - errors/failures */
} Carbon_NotifyType;

/**
 * Individual notification data
 */
typedef struct {
    char message[CARBON_NOTIFICATION_MAX_LEN]; /**< Message text */
    float time_remaining;                       /**< Seconds until expiration */
    float r, g, b, a;                          /**< Color (RGBA 0.0-1.0) */
    Carbon_NotifyType type;                    /**< Notification type */
} Carbon_Notification;

/**
 * Notification manager (opaque structure)
 */
typedef struct Carbon_NotificationManager Carbon_NotificationManager;

/**
 * Create a notification manager.
 *
 * @return New notification manager, or NULL on failure
 */
Carbon_NotificationManager *carbon_notify_create(void);

/**
 * Destroy a notification manager and free resources.
 *
 * @param mgr Notification manager to destroy
 */
void carbon_notify_destroy(Carbon_NotificationManager *mgr);

/**
 * Add a notification with default duration.
 *
 * @param mgr Notification manager
 * @param message Message text (truncated if too long)
 * @param type Notification type (determines color)
 */
void carbon_notify_add(Carbon_NotificationManager *mgr, const char *message, Carbon_NotifyType type);

/**
 * Add a notification with custom duration.
 *
 * @param mgr Notification manager
 * @param message Message text
 * @param type Notification type
 * @param duration Duration in seconds before auto-removal
 */
void carbon_notify_add_timed(Carbon_NotificationManager *mgr, const char *message,
                              Carbon_NotifyType type, float duration);

/**
 * Add a notification with custom color.
 *
 * @param mgr Notification manager
 * @param message Message text
 * @param r Red component (0.0-1.0)
 * @param g Green component (0.0-1.0)
 * @param b Blue component (0.0-1.0)
 */
void carbon_notify_add_colored(Carbon_NotificationManager *mgr, const char *message,
                                float r, float g, float b);

/**
 * Add a notification with printf-style formatting.
 *
 * @param mgr Notification manager
 * @param type Notification type
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void carbon_notify_printf(Carbon_NotificationManager *mgr, Carbon_NotifyType type,
                          const char *fmt, ...);

/**
 * Add a notification with printf-style formatting (va_list version).
 *
 * @param mgr Notification manager
 * @param type Notification type
 * @param fmt Printf-style format string
 * @param args Format arguments
 */
void carbon_notify_printf_v(Carbon_NotificationManager *mgr, Carbon_NotifyType type,
                            const char *fmt, va_list args);

/**
 * Update all notifications (call each frame).
 * Removes expired notifications.
 *
 * @param mgr Notification manager
 * @param dt Delta time in seconds
 */
void carbon_notify_update(Carbon_NotificationManager *mgr, float dt);

/**
 * Clear all notifications.
 *
 * @param mgr Notification manager
 */
void carbon_notify_clear(Carbon_NotificationManager *mgr);

/**
 * Get the number of active notifications.
 *
 * @param mgr Notification manager
 * @return Number of notifications currently displayed
 */
int carbon_notify_count(Carbon_NotificationManager *mgr);

/**
 * Get a notification by index.
 *
 * @param mgr Notification manager
 * @param index Notification index (0 = oldest, count-1 = newest)
 * @return Pointer to notification data, or NULL if invalid index
 */
const Carbon_Notification *carbon_notify_get(Carbon_NotificationManager *mgr, int index);

/**
 * Set the default notification duration.
 *
 * @param mgr Notification manager
 * @param duration Duration in seconds
 */
void carbon_notify_set_default_duration(Carbon_NotificationManager *mgr, float duration);

/**
 * Get the default notification duration.
 *
 * @param mgr Notification manager
 * @return Default duration in seconds
 */
float carbon_notify_get_default_duration(Carbon_NotificationManager *mgr);

/**
 * Set whether newer notifications appear at top (default) or bottom.
 *
 * @param mgr Notification manager
 * @param newest_on_top true = newest at top, false = newest at bottom
 */
void carbon_notify_set_newest_on_top(Carbon_NotificationManager *mgr, bool newest_on_top);

/*============================================================================
 * Rendering Integration
 *============================================================================*/

/* Forward declarations for optional rendering integration */
struct Carbon_TextRenderer;
struct Carbon_Font;

/**
 * Render notifications using the Carbon text renderer.
 * Call this during your text batch (between carbon_text_begin/end).
 *
 * @param mgr Notification manager
 * @param text Text renderer
 * @param font Font to use
 * @param x X position of notification stack
 * @param y Y position of notification stack (top if newest_on_top, bottom otherwise)
 * @param spacing Vertical spacing between notifications
 */
void carbon_notify_render(Carbon_NotificationManager *mgr,
                          struct Carbon_TextRenderer *text,
                          struct Carbon_Font *font,
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
void carbon_notify_get_type_color(Carbon_NotifyType type, float *r, float *g, float *b);

#endif /* CARBON_NOTIFICATION_H */
