#ifndef CARBON_HELPERS_H
#define CARBON_HELPERS_H

#include "carbon/game_context.h"
#include <math.h>

/**
 * Carbon Helper Macros and Utilities
 *
 * Provides convenience macros for common patterns and utility functions
 * for coordinate conversion, timing, and math operations.
 */

/*============================================================================
 * Sprite Batching Helpers
 *============================================================================*/

/**
 * Begin a sprite batch, draw sprites, upload, and prepare for rendering.
 *
 * Usage:
 *   CARBON_SPRITE_BATCH_BEGIN(ctx) {
 *       carbon_sprite_draw(ctx->sprites, &sprite, x, y);
 *       // ... more draws ...
 *   } CARBON_SPRITE_BATCH_END(ctx, cmd);
 *
 *   // During render pass:
 *   carbon_sprite_render(ctx->sprites, cmd, pass);
 */
#define CARBON_SPRITE_BATCH_BEGIN(ctx) \
    carbon_sprite_begin((ctx)->sprites, NULL)

#define CARBON_SPRITE_BATCH_END(ctx, cmd) \
    carbon_sprite_upload((ctx)->sprites, (cmd))

/**
 * Complete sprite batch helper for simple cases.
 * Draws a single sprite with transform.
 */
#define CARBON_DRAW_SPRITE(ctx, sprite, x, y) \
    carbon_sprite_draw((ctx)->sprites, (sprite), (x), (y))

#define CARBON_DRAW_SPRITE_SCALED(ctx, sprite, x, y, sx, sy) \
    carbon_sprite_draw_scaled((ctx)->sprites, (sprite), (x), (y), (sx), (sy))

#define CARBON_DRAW_SPRITE_EX(ctx, sprite, x, y, sx, sy, rot, ox, oy) \
    carbon_sprite_draw_ex((ctx)->sprites, (sprite), (x), (y), (sx), (sy), (rot), (ox), (oy))

/*============================================================================
 * Text Rendering Helpers
 *============================================================================*/

/**
 * Begin a text batch, draw text, and upload.
 *
 * Usage:
 *   CARBON_TEXT_BATCH_BEGIN(ctx) {
 *       carbon_text_draw(ctx->text, ctx->font, "Hello", x, y);
 *   } CARBON_TEXT_BATCH_END(ctx, cmd);
 */
#define CARBON_TEXT_BATCH_BEGIN(ctx) \
    carbon_text_begin((ctx)->text)

#define CARBON_TEXT_BATCH_END(ctx, cmd) \
    do { \
        carbon_text_end((ctx)->text); \
        carbon_text_upload((ctx)->text, (cmd)); \
    } while(0)

/**
 * Simple text drawing using context's default font.
 */
#define CARBON_DRAW_TEXT(ctx, str, x, y) \
    carbon_text_draw((ctx)->text, (ctx)->font, (str), (x), (y))

#define CARBON_DRAW_TEXT_COLORED(ctx, str, x, y, r, g, b, a) \
    carbon_text_draw_colored((ctx)->text, (ctx)->font, (str), (x), (y), (r), (g), (b), (a))

/*============================================================================
 * UI Helpers
 *============================================================================*/

/**
 * Begin UI frame, draw widgets, upload and prepare for rendering.
 *
 * Usage:
 *   CARBON_UI_BEGIN(ctx) {
 *       if (cui_begin_panel(ctx->ui, "Panel", ...)) {
 *           cui_button(ctx->ui, "Click Me");
 *           cui_end_panel(ctx->ui);
 *       }
 *   } CARBON_UI_END(ctx, cmd);
 */
#define CARBON_UI_BEGIN(ctx) \
    cui_begin_frame((ctx)->ui, (ctx)->delta_time)

#define CARBON_UI_END(ctx, cmd) \
    do { \
        cui_end_frame((ctx)->ui); \
        cui_upload((ctx)->ui, (cmd)); \
    } while(0)

/*============================================================================
 * Input Helpers
 *============================================================================*/

/**
 * Check if action was just pressed this frame.
 */
#define CARBON_ACTION_JUST_PRESSED(ctx, action_id) \
    carbon_input_action_just_pressed((ctx)->input, (action_id))

/**
 * Check if action is currently held.
 */
#define CARBON_ACTION_PRESSED(ctx, action_id) \
    carbon_input_action_pressed((ctx)->input, (action_id))

/**
 * Check if action was just released this frame.
 */
#define CARBON_ACTION_JUST_RELEASED(ctx, action_id) \
    carbon_input_action_just_released((ctx)->input, (action_id))

/**
 * Get analog value of action (for triggers/sticks).
 */
#define CARBON_ACTION_VALUE(ctx, action_id) \
    carbon_input_action_value((ctx)->input, (action_id))

/*============================================================================
 * Math Utilities
 *============================================================================*/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Convert degrees to radians.
 */
static inline float carbon_deg_to_rad(float degrees) {
    return degrees * (float)(M_PI / 180.0);
}

/**
 * Convert radians to degrees.
 */
static inline float carbon_rad_to_deg(float radians) {
    return radians * (float)(180.0 / M_PI);
}

/**
 * Linear interpolation.
 */
static inline float carbon_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/**
 * Clamp value between min and max.
 */
static inline float carbon_clamp(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * Clamp integer value between min and max.
 */
static inline int carbon_clamp_i(int value, int min_val, int max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * Smooth step interpolation (cubic Hermite).
 */
static inline float carbon_smoothstep(float edge0, float edge1, float x) {
    float t = carbon_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * Sign of a value (-1, 0, or 1).
 */
static inline int carbon_sign(float value) {
    if (value > 0) return 1;
    if (value < 0) return -1;
    return 0;
}

/**
 * Distance between two points.
 */
static inline float carbon_distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

/**
 * Squared distance between two points (faster, good for comparisons).
 */
static inline float carbon_distance_squared(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;
}

/**
 * Normalize angle to 0-360 range.
 */
static inline float carbon_normalize_angle(float degrees) {
    while (degrees < 0) degrees += 360.0f;
    while (degrees >= 360.0f) degrees -= 360.0f;
    return degrees;
}

/**
 * Angle from point 1 to point 2 in degrees.
 */
static inline float carbon_angle_to(float x1, float y1, float x2, float y2) {
    return carbon_rad_to_deg(atan2f(y2 - y1, x2 - x1));
}

/*============================================================================
 * Coordinate Conversion Helpers
 *============================================================================*/

/**
 * Convert screen coordinates to world coordinates using context's camera.
 */
static inline void carbon_screen_to_world(Carbon_GameContext *ctx,
                                           float screen_x, float screen_y,
                                           float *world_x, float *world_y) {
    carbon_camera_screen_to_world(ctx->camera, screen_x, screen_y, world_x, world_y);
}

/**
 * Convert world coordinates to screen coordinates using context's camera.
 */
static inline void carbon_world_to_screen(Carbon_GameContext *ctx,
                                           float world_x, float world_y,
                                           float *screen_x, float *screen_y) {
    carbon_camera_world_to_screen(ctx->camera, world_x, world_y, screen_x, screen_y);
}

/*============================================================================
 * Timing Utilities
 *============================================================================*/

/**
 * Simple timer structure for delays and intervals.
 */
typedef struct {
    float elapsed;
    float duration;
    bool finished;
} Carbon_Timer;

/**
 * Initialize a timer with a duration in seconds.
 */
static inline void carbon_timer_init(Carbon_Timer *timer, float duration) {
    timer->elapsed = 0.0f;
    timer->duration = duration;
    timer->finished = false;
}

/**
 * Update a timer. Returns true when it finishes (once).
 */
static inline bool carbon_timer_update(Carbon_Timer *timer, float dt) {
    if (timer->finished) return false;
    timer->elapsed += dt;
    if (timer->elapsed >= timer->duration) {
        timer->finished = true;
        return true;
    }
    return false;
}

/**
 * Reset a timer to run again.
 */
static inline void carbon_timer_reset(Carbon_Timer *timer) {
    timer->elapsed = 0.0f;
    timer->finished = false;
}

/**
 * Get progress of timer (0.0 to 1.0).
 */
static inline float carbon_timer_progress(const Carbon_Timer *timer) {
    if (timer->duration <= 0) return 1.0f;
    return carbon_clamp(timer->elapsed / timer->duration, 0.0f, 1.0f);
}

/*============================================================================
 * Random Number Utilities
 *============================================================================*/

/**
 * Get a random float between 0.0 and 1.0.
 */
static inline float carbon_random_float(void) {
    return (float)rand() / (float)RAND_MAX;
}

/**
 * Get a random float in range [min, max].
 */
static inline float carbon_random_range(float min_val, float max_val) {
    return min_val + carbon_random_float() * (max_val - min_val);
}

/**
 * Get a random integer in range [min, max] (inclusive).
 */
static inline int carbon_random_int(int min_val, int max_val) {
    return min_val + rand() % (max_val - min_val + 1);
}

#endif /* CARBON_HELPERS_H */
