/*
 * Agentite Gizmo Rendering System
 *
 * Provides interactive transform handles and debug visualization:
 * - Transform gizmos: translate, rotate, scale handles
 * - Debug drawing: lines, arrows, boxes, spheres, circles
 * - Screen-space overlays: 2D primitives, grids
 *
 * Usage:
 *   Agentite_Gizmos *gizmos = agentite_gizmos_create(gpu, NULL);
 *
 *   // Each frame (before render pass):
 *   agentite_gizmos_begin(gizmos, camera);
 *   agentite_gizmos_update_input(gizmos, mouse_x, mouse_y, mouse_down, mouse_pressed);
 *
 *   // Draw transform gizmo
 *   vec3 pos = {0, 0, 0};
 *   Agentite_GizmoResult result = agentite_gizmo_translate(gizmos, pos, NULL);
 *   if (result.active) {
 *       pos[0] += result.delta[0];
 *       pos[1] += result.delta[1];
 *       pos[2] += result.delta[2];
 *   }
 *
 *   // Debug visualization
 *   agentite_gizmos_line(gizmos, start, end, 0xFF0000FF);
 *   agentite_gizmos_box(gizmos, center, size, 0x00FF00FF);
 *
 *   agentite_gizmos_end(gizmos);
 *   agentite_gizmos_upload(gizmos, cmd);
 *
 *   // During render pass:
 *   agentite_gizmos_render(gizmos, cmd, pass);
 *
 *   agentite_gizmos_destroy(gizmos);
 */

#ifndef AGENTITE_GIZMOS_H
#define AGENTITE_GIZMOS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <cglm/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Opaque gizmo renderer context */
typedef struct Agentite_Gizmos Agentite_Gizmos;

/* Forward declaration */
typedef struct Agentite_Camera Agentite_Camera;

/* Gizmo operation modes */
typedef enum Agentite_GizmoMode {
    AGENTITE_GIZMO_TRANSLATE,
    AGENTITE_GIZMO_ROTATE,
    AGENTITE_GIZMO_SCALE,
    AGENTITE_GIZMO_BOUNDS
} Agentite_GizmoMode;

/* Gizmo axis flags */
typedef enum Agentite_GizmoAxis {
    AGENTITE_AXIS_NONE = 0,
    AGENTITE_AXIS_X = 1 << 0,
    AGENTITE_AXIS_Y = 1 << 1,
    AGENTITE_AXIS_Z = 1 << 2,
    AGENTITE_AXIS_XY = AGENTITE_AXIS_X | AGENTITE_AXIS_Y,
    AGENTITE_AXIS_XZ = AGENTITE_AXIS_X | AGENTITE_AXIS_Z,
    AGENTITE_AXIS_YZ = AGENTITE_AXIS_Y | AGENTITE_AXIS_Z,
    AGENTITE_AXIS_ALL = AGENTITE_AXIS_X | AGENTITE_AXIS_Y | AGENTITE_AXIS_Z
} Agentite_GizmoAxis;

/* Colors for gizmo axes */
typedef struct Agentite_GizmoColors {
    uint32_t x_color;       /* Red default (0xFF0000FF) */
    uint32_t y_color;       /* Green default (0x00FF00FF) */
    uint32_t z_color;       /* Blue default (0x0000FFFF) */
    uint32_t hover_color;   /* Yellow default (0xFFFF00FF) */
    uint32_t active_color;  /* White default (0xFFFFFFFF) */
} Agentite_GizmoColors;

#define AGENTITE_GIZMO_COLORS_DEFAULT { \
    .x_color = 0xFF0000FF,              \
    .y_color = 0x00FF00FF,              \
    .z_color = 0x0000FFFF,              \
    .hover_color = 0xFFFF00FF,          \
    .active_color = 0xFFFFFFFF          \
}

/* Gizmo configuration */
typedef struct Agentite_GizmoConfig {
    float handle_size;         /* Size in pixels (default: 100) */
    float line_thickness;      /* Line width (default: 2) */
    float hover_threshold;     /* Pixels for hover detection (default: 8) */
    bool depth_test;           /* Respect depth buffer (default: false) */
    bool screen_space_size;    /* Keep constant screen size (default: true) */
    Agentite_GizmoColors colors;
} Agentite_GizmoConfig;

#define AGENTITE_GIZMO_CONFIG_DEFAULT { \
    .handle_size = 100.0f,              \
    .line_thickness = 2.0f,             \
    .hover_threshold = 16.0f,           \
    .depth_test = false,                \
    .screen_space_size = true,          \
    .colors = AGENTITE_GIZMO_COLORS_DEFAULT \
}

/* Result from interactive gizmo operations */
typedef struct Agentite_GizmoResult {
    bool hovered;              /* Any axis hovered */
    bool active;               /* Currently dragging */
    Agentite_GizmoAxis axis;   /* Which axis is hovered/active */
    vec3 delta;                /* Change this frame (in world units) */
    float angle_delta;         /* For rotation (radians) */
} Agentite_GizmoResult;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Create gizmo renderer.
 * Caller OWNS the returned pointer and MUST call agentite_gizmos_destroy().
 *
 * @param device    GPU device (required)
 * @param config    Configuration (NULL for defaults)
 * @return          Gizmo renderer or NULL on failure
 */
Agentite_Gizmos *agentite_gizmos_create(SDL_GPUDevice *device,
                                         const Agentite_GizmoConfig *config);

/**
 * Destroy gizmo renderer and free all resources.
 * Safe to call with NULL.
 */
void agentite_gizmos_destroy(Agentite_Gizmos *gizmos);

/* ============================================================================
 * Frame Management
 * ============================================================================ */

/**
 * Begin gizmo frame. Call before drawing any gizmos.
 * Clears previous frame's primitives.
 *
 * @param gizmos    Gizmo renderer
 * @param camera    Camera for 3D→screen projection (NULL for 2D only)
 */
void agentite_gizmos_begin(Agentite_Gizmos *gizmos, Agentite_Camera *camera);

/**
 * End gizmo frame. Call after all drawing is complete.
 */
void agentite_gizmos_end(Agentite_Gizmos *gizmos);

/**
 * Set screen dimensions (call when window resizes).
 */
void agentite_gizmos_set_screen_size(Agentite_Gizmos *gizmos, int width, int height);

/**
 * Upload gizmo data to GPU. Call BEFORE render pass begins.
 *
 * @param gizmos    Gizmo renderer
 * @param cmd       Command buffer
 */
void agentite_gizmos_upload(Agentite_Gizmos *gizmos, SDL_GPUCommandBuffer *cmd);

/**
 * Render gizmos. Call DURING render pass.
 *
 * @param gizmos    Gizmo renderer
 * @param cmd       Command buffer
 * @param pass      Render pass
 */
void agentite_gizmos_render(Agentite_Gizmos *gizmos, SDL_GPUCommandBuffer *cmd,
                            SDL_GPURenderPass *pass);

/* ============================================================================
 * Input Handling
 * ============================================================================ */

/**
 * Update input state for gizmo interaction.
 * Call once per frame after agentite_gizmos_begin().
 *
 * @param gizmos        Gizmo renderer
 * @param mouse_x       Mouse X in screen coordinates
 * @param mouse_y       Mouse Y in screen coordinates
 * @param mouse_down    True if mouse button is held
 * @param mouse_pressed True if mouse button was just pressed this frame
 */
void agentite_gizmos_update_input(Agentite_Gizmos *gizmos,
                                   float mouse_x, float mouse_y,
                                   bool mouse_down, bool mouse_pressed);

/**
 * Check if gizmo consumed input (editor should skip scene interaction).
 */
bool agentite_gizmos_is_active(Agentite_Gizmos *gizmos);

/**
 * Check if any gizmo is hovered.
 */
bool agentite_gizmos_is_hovered(Agentite_Gizmos *gizmos);

/* ============================================================================
 * Transform Gizmos (Interactive)
 * ============================================================================ */

/**
 * Draw translation gizmo (arrows on each axis).
 *
 * @param gizmos        Gizmo renderer
 * @param position      World position of gizmo center
 * @param orientation   Optional rotation matrix (NULL = world axes)
 * @return              Interaction result with delta movement
 */
Agentite_GizmoResult agentite_gizmo_translate(Agentite_Gizmos *gizmos,
                                               vec3 position,
                                               mat4 *orientation);

/**
 * Draw rotation gizmo (circles around each axis).
 *
 * @param gizmos        Gizmo renderer
 * @param position      World position of gizmo center
 * @param orientation   Optional rotation matrix (NULL = world axes)
 * @return              Interaction result with angle_delta in radians
 */
Agentite_GizmoResult agentite_gizmo_rotate(Agentite_Gizmos *gizmos,
                                            vec3 position,
                                            mat4 *orientation);

/**
 * Draw scale gizmo (boxes on each axis).
 *
 * @param gizmos        Gizmo renderer
 * @param position      World position of gizmo center
 * @param orientation   Optional rotation matrix (NULL = world axes)
 * @return              Interaction result with scale delta
 */
Agentite_GizmoResult agentite_gizmo_scale(Agentite_Gizmos *gizmos,
                                           vec3 position,
                                           mat4 *orientation);

/**
 * Draw combined transform gizmo (switches based on mode).
 *
 * @param gizmos        Gizmo renderer
 * @param mode          Which transform to show
 * @param position      World position of gizmo center
 * @param orientation   Optional rotation matrix (NULL = world axes)
 * @return              Interaction result
 */
Agentite_GizmoResult agentite_gizmo_transform(Agentite_Gizmos *gizmos,
                                               Agentite_GizmoMode mode,
                                               vec3 position,
                                               mat4 *orientation);

/* ============================================================================
 * Debug Drawing - 3D World Space
 * ============================================================================ */

/**
 * Draw a line between two points.
 *
 * @param gizmos    Gizmo renderer
 * @param from      Start point (world space)
 * @param to        End point (world space)
 * @param color     RGBA color (0xRRGGBBAA)
 */
void agentite_gizmos_line(Agentite_Gizmos *gizmos, vec3 from, vec3 to, uint32_t color);

/**
 * Draw a ray from origin in a direction.
 *
 * @param gizmos    Gizmo renderer
 * @param origin    Ray origin (world space)
 * @param dir       Ray direction (normalized)
 * @param length    Ray length
 * @param color     RGBA color
 */
void agentite_gizmos_ray(Agentite_Gizmos *gizmos, vec3 origin, vec3 dir,
                          float length, uint32_t color);

/**
 * Draw an arrow (line with arrowhead).
 *
 * @param gizmos    Gizmo renderer
 * @param from      Arrow start (world space)
 * @param to        Arrow end (world space)
 * @param color     RGBA color
 */
void agentite_gizmos_arrow(Agentite_Gizmos *gizmos, vec3 from, vec3 to, uint32_t color);

/**
 * Draw a wireframe box.
 *
 * @param gizmos    Gizmo renderer
 * @param center    Box center (world space)
 * @param size      Box dimensions (width, height, depth)
 * @param color     RGBA color
 */
void agentite_gizmos_box(Agentite_Gizmos *gizmos, vec3 center, vec3 size, uint32_t color);

/**
 * Draw a wireframe sphere.
 *
 * @param gizmos    Gizmo renderer
 * @param center    Sphere center (world space)
 * @param radius    Sphere radius
 * @param color     RGBA color
 */
void agentite_gizmos_sphere(Agentite_Gizmos *gizmos, vec3 center, float radius, uint32_t color);

/**
 * Draw a circle.
 *
 * @param gizmos    Gizmo renderer
 * @param center    Circle center (world space)
 * @param normal    Circle normal (defines plane)
 * @param radius    Circle radius
 * @param color     RGBA color
 */
void agentite_gizmos_circle(Agentite_Gizmos *gizmos, vec3 center, vec3 normal,
                             float radius, uint32_t color);

/**
 * Draw an arc (partial circle).
 *
 * @param gizmos    Gizmo renderer
 * @param center    Arc center (world space)
 * @param normal    Arc normal (defines plane)
 * @param from      Starting direction on the plane
 * @param angle     Arc angle in radians
 * @param radius    Arc radius
 * @param color     RGBA color
 */
void agentite_gizmos_arc(Agentite_Gizmos *gizmos, vec3 center, vec3 normal,
                          vec3 from, float angle, float radius, uint32_t color);

/**
 * Draw an axis-aligned bounding box.
 *
 * @param gizmos    Gizmo renderer
 * @param min       Minimum corner (world space)
 * @param max       Maximum corner (world space)
 * @param color     RGBA color
 */
void agentite_gizmos_bounds(Agentite_Gizmos *gizmos, vec3 min, vec3 max, uint32_t color);

/**
 * Draw a 3D grid.
 *
 * @param gizmos    Gizmo renderer
 * @param center    Grid center (world space)
 * @param normal    Grid normal (defines plane)
 * @param size      Total grid size
 * @param spacing   Distance between grid lines
 * @param color     RGBA color
 */
void agentite_gizmos_grid(Agentite_Gizmos *gizmos, vec3 center, vec3 normal,
                           float size, float spacing, uint32_t color);

/* ============================================================================
 * Debug Drawing - 2D Screen Space
 * ============================================================================ */

/**
 * Draw a 2D line in screen space.
 *
 * @param gizmos    Gizmo renderer
 * @param x1, y1    Start point (screen pixels)
 * @param x2, y2    End point (screen pixels)
 * @param color     RGBA color
 */
void agentite_gizmos_line_2d(Agentite_Gizmos *gizmos, float x1, float y1,
                              float x2, float y2, uint32_t color);

/**
 * Draw a 2D rectangle outline in screen space.
 *
 * @param gizmos    Gizmo renderer
 * @param x, y      Top-left corner (screen pixels)
 * @param w, h      Width and height
 * @param color     RGBA color
 */
void agentite_gizmos_rect_2d(Agentite_Gizmos *gizmos, float x, float y,
                              float w, float h, uint32_t color);

/**
 * Draw a 2D filled rectangle in screen space.
 *
 * @param gizmos    Gizmo renderer
 * @param x, y      Top-left corner (screen pixels)
 * @param w, h      Width and height
 * @param color     RGBA color
 */
void agentite_gizmos_rect_filled_2d(Agentite_Gizmos *gizmos, float x, float y,
                                     float w, float h, uint32_t color);

/**
 * Draw a 2D circle outline in screen space.
 *
 * @param gizmos    Gizmo renderer
 * @param x, y      Center (screen pixels)
 * @param radius    Circle radius
 * @param color     RGBA color
 */
void agentite_gizmos_circle_2d(Agentite_Gizmos *gizmos, float x, float y,
                                float radius, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_GIZMOS_H */
