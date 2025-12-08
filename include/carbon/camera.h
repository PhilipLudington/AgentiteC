/*
 * Carbon 2D Camera System
 *
 * Provides view/projection matrix for sprite rendering with:
 * - Position (pan)
 * - Zoom
 * - Rotation
 * - Screen-to-world / world-to-screen coordinate conversion
 */

#ifndef CARBON_CAMERA_H
#define CARBON_CAMERA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque camera handle */
typedef struct Carbon_Camera Carbon_Camera;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/* Create camera with viewport dimensions */
Carbon_Camera *carbon_camera_create(float viewport_w, float viewport_h);

/* Destroy camera */
void carbon_camera_destroy(Carbon_Camera *camera);

/* ============================================================================
 * Transform Setters
 * ============================================================================ */

/* Set camera position (world coordinates of view center) */
void carbon_camera_set_position(Carbon_Camera *cam, float x, float y);

/* Move camera by delta in world units */
void carbon_camera_move(Carbon_Camera *cam, float dx, float dy);

/* Set zoom level (1.0 = normal, 2.0 = 2x magnification) */
void carbon_camera_set_zoom(Carbon_Camera *cam, float zoom);

/* Set rotation in degrees */
void carbon_camera_set_rotation(Carbon_Camera *cam, float degrees);

/* Update viewport dimensions (call on window resize) */
void carbon_camera_set_viewport(Carbon_Camera *cam, float w, float h);

/* ============================================================================
 * Getters
 * ============================================================================ */

/* Get camera position */
void carbon_camera_get_position(Carbon_Camera *cam, float *x, float *y);

/* Get zoom level */
float carbon_camera_get_zoom(Carbon_Camera *cam);

/* Get rotation in degrees */
float carbon_camera_get_rotation(Carbon_Camera *cam);

/* Get viewport dimensions */
void carbon_camera_get_viewport(Carbon_Camera *cam, float *w, float *h);

/* ============================================================================
 * Matrix Access (for sprite renderer)
 * ============================================================================ */

/* Recompute matrices if dirty (call once per frame before rendering) */
void carbon_camera_update(Carbon_Camera *cam);

/* Get the combined view-projection matrix (16 floats, column-major) */
const float *carbon_camera_get_vp_matrix(Carbon_Camera *cam);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/* Convert screen coordinates to world coordinates (for mouse picking) */
void carbon_camera_screen_to_world(Carbon_Camera *cam,
                                   float screen_x, float screen_y,
                                   float *world_x, float *world_y);

/* Convert world coordinates to screen coordinates */
void carbon_camera_world_to_screen(Carbon_Camera *cam,
                                   float world_x, float world_y,
                                   float *screen_x, float *screen_y);

/* Get visible world bounds (AABB) */
void carbon_camera_get_bounds(Carbon_Camera *cam,
                              float *left, float *right,
                              float *top, float *bottom);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_CAMERA_H */
