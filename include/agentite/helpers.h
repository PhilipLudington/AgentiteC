#ifndef AGENTITE_HELPERS_H
#define AGENTITE_HELPERS_H

#include "agentite/game_context.h"
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
 *   AGENTITE_SPRITE_BATCH_BEGIN(ctx) {
 *       agentite_sprite_draw(ctx->sprites, &sprite, x, y);
 *       // ... more draws ...
 *   } AGENTITE_SPRITE_BATCH_END(ctx, cmd);
 *
 *   // During render pass:
 *   agentite_sprite_render(ctx->sprites, cmd, pass);
 */
#define AGENTITE_SPRITE_BATCH_BEGIN(ctx) \
    agentite_sprite_begin((ctx)->sprites, NULL)

#define AGENTITE_SPRITE_BATCH_END(ctx, cmd) \
    agentite_sprite_upload((ctx)->sprites, (cmd))

/**
 * Complete sprite batch helper for simple cases.
 * Draws a single sprite with transform.
 */
#define AGENTITE_DRAW_SPRITE(ctx, sprite, x, y) \
    agentite_sprite_draw((ctx)->sprites, (sprite), (x), (y))

#define AGENTITE_DRAW_SPRITE_SCALED(ctx, sprite, x, y, sx, sy) \
    agentite_sprite_draw_scaled((ctx)->sprites, (sprite), (x), (y), (sx), (sy))

#define AGENTITE_DRAW_SPRITE_EX(ctx, sprite, x, y, sx, sy, rot, ox, oy) \
    agentite_sprite_draw_ex((ctx)->sprites, (sprite), (x), (y), (sx), (sy), (rot), (ox), (oy))

/*============================================================================
 * Text Rendering Helpers
 *============================================================================*/

/**
 * Begin a text batch, draw text, and upload.
 *
 * Usage:
 *   AGENTITE_TEXT_BATCH_BEGIN(ctx) {
 *       agentite_text_draw(ctx->text, ctx->font, "Hello", x, y);
 *   } AGENTITE_TEXT_BATCH_END(ctx, cmd);
 */
#define AGENTITE_TEXT_BATCH_BEGIN(ctx) \
    agentite_text_begin((ctx)->text)

#define AGENTITE_TEXT_BATCH_END(ctx, cmd) \
    do { \
        agentite_text_end((ctx)->text); \
        agentite_text_upload((ctx)->text, (cmd)); \
    } while(0)

/**
 * Simple text drawing using context's default font.
 */
#define AGENTITE_DRAW_TEXT(ctx, str, x, y) \
    agentite_text_draw((ctx)->text, (ctx)->font, (str), (x), (y))

#define AGENTITE_DRAW_TEXT_COLORED(ctx, str, x, y, r, g, b, a) \
    agentite_text_draw_colored((ctx)->text, (ctx)->font, (str), (x), (y), (r), (g), (b), (a))

/*============================================================================
 * UI Helpers
 *============================================================================*/

/**
 * Begin UI frame, draw widgets, upload and prepare for rendering.
 *
 * Usage:
 *   AGENTITE_UI_BEGIN(ctx) {
 *       if (aui_begin_panel(ctx->ui, "Panel", ...)) {
 *           aui_button(ctx->ui, "Click Me");
 *           aui_end_panel(ctx->ui);
 *       }
 *   } AGENTITE_UI_END(ctx, cmd);
 */
#define AGENTITE_UI_BEGIN(ctx) \
    aui_begin_frame((ctx)->ui, (ctx)->delta_time)

#define AGENTITE_UI_END(ctx, cmd) \
    do { \
        aui_end_frame((ctx)->ui); \
        aui_upload((ctx)->ui, (cmd)); \
    } while(0)

/*============================================================================
 * Input Helpers
 *============================================================================*/

/**
 * Check if action was just pressed this frame.
 */
#define AGENTITE_ACTION_JUST_PRESSED(ctx, action_id) \
    agentite_input_action_just_pressed((ctx)->input, (action_id))

/**
 * Check if action is currently held.
 */
#define AGENTITE_ACTION_PRESSED(ctx, action_id) \
    agentite_input_action_pressed((ctx)->input, (action_id))

/**
 * Check if action was just released this frame.
 */
#define AGENTITE_ACTION_JUST_RELEASED(ctx, action_id) \
    agentite_input_action_just_released((ctx)->input, (action_id))

/**
 * Get analog value of action (for triggers/sticks).
 */
#define AGENTITE_ACTION_VALUE(ctx, action_id) \
    agentite_input_action_value((ctx)->input, (action_id))

/*============================================================================
 * Math Utilities
 *============================================================================*/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Convert degrees to radians.
 */
static inline float agentite_deg_to_rad(float degrees) {
    return degrees * (float)(M_PI / 180.0);
}

/**
 * Convert radians to degrees.
 */
static inline float agentite_rad_to_deg(float radians) {
    return radians * (float)(180.0 / M_PI);
}

/**
 * Linear interpolation.
 */
static inline float agentite_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/**
 * Clamp value between min and max.
 */
static inline float agentite_clamp(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * Clamp integer value between min and max.
 */
static inline int agentite_clamp_i(int value, int min_val, int max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * Smooth step interpolation (cubic Hermite).
 */
static inline float agentite_smoothstep(float edge0, float edge1, float x) {
    float t = agentite_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * Sign of a value (-1, 0, or 1).
 */
static inline int agentite_sign(float value) {
    if (value > 0) return 1;
    if (value < 0) return -1;
    return 0;
}

/**
 * Distance between two points.
 */
static inline float agentite_distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

/**
 * Squared distance between two points (faster, good for comparisons).
 */
static inline float agentite_distance_squared(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;
}

/**
 * Normalize angle to 0-360 range.
 */
static inline float agentite_normalize_angle(float degrees) {
    while (degrees < 0) degrees += 360.0f;
    while (degrees >= 360.0f) degrees -= 360.0f;
    return degrees;
}

/**
 * Angle from point 1 to point 2 in degrees.
 */
static inline float agentite_angle_to(float x1, float y1, float x2, float y2) {
    return agentite_rad_to_deg(atan2f(y2 - y1, x2 - x1));
}

/*============================================================================
 * Coordinate Conversion Helpers
 *============================================================================*/

/**
 * Convert screen coordinates to world coordinates using context's camera.
 */
static inline void agentite_screen_to_world(Agentite_GameContext *ctx,
                                           float screen_x, float screen_y,
                                           float *world_x, float *world_y) {
    agentite_camera_screen_to_world(ctx->camera, screen_x, screen_y, world_x, world_y);
}

/**
 * Convert world coordinates to screen coordinates using context's camera.
 */
static inline void agentite_world_to_screen(Agentite_GameContext *ctx,
                                           float world_x, float world_y,
                                           float *screen_x, float *screen_y) {
    agentite_camera_world_to_screen(ctx->camera, world_x, world_y, screen_x, screen_y);
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
} Agentite_Timer;

/**
 * Initialize a timer with a duration in seconds.
 */
static inline void agentite_timer_init(Agentite_Timer *timer, float duration) {
    timer->elapsed = 0.0f;
    timer->duration = duration;
    timer->finished = false;
}

/**
 * Update a timer. Returns true when it finishes (once).
 */
static inline bool agentite_timer_update(Agentite_Timer *timer, float dt) {
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
static inline void agentite_timer_reset(Agentite_Timer *timer) {
    timer->elapsed = 0.0f;
    timer->finished = false;
}

/**
 * Get progress of timer (0.0 to 1.0).
 */
static inline float agentite_timer_progress(const Agentite_Timer *timer) {
    if (timer->duration <= 0) return 1.0f;
    return agentite_clamp(timer->elapsed / timer->duration, 0.0f, 1.0f);
}

/*============================================================================
 * Random Number Utilities
 *============================================================================*/

/**
 * Get a random float between 0.0 and 1.0.
 */
static inline float agentite_random_float(void) {
    return (float)rand() / (float)RAND_MAX;
}

/**
 * Get a random float in range [min, max].
 */
static inline float agentite_random_range(float min_val, float max_val) {
    return min_val + agentite_random_float() * (max_val - min_val);
}

/**
 * Get a random integer in range [min, max] (inclusive).
 */
static inline int agentite_random_int(int min_val, int max_val) {
    return min_val + rand() % (max_val - min_val + 1);
}

#endif /* AGENTITE_HELPERS_H */
