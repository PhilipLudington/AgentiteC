/*
 * Carbon 2D Camera System
 *
 * Provides view/projection matrix for sprite rendering with:
 * - Position (pan)
 * - Zoom
 * - Rotation
 * - Screen-to-world / world-to-screen coordinate conversion
 */

#ifndef AGENTITE_CAMERA_H
#define AGENTITE_CAMERA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque camera handle */
typedef struct Agentite_Camera Agentite_Camera;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/* Create camera with viewport dimensions */
Agentite_Camera *agentite_camera_create(float viewport_w, float viewport_h);

/* Destroy camera */
void agentite_camera_destroy(Agentite_Camera *camera);

/* ============================================================================
 * Transform Setters
 * ============================================================================ */

/* Set camera position (world coordinates of view center) */
void agentite_camera_set_position(Agentite_Camera *cam, float x, float y);

/* Move camera by delta in world units */
void agentite_camera_move(Agentite_Camera *cam, float dx, float dy);

/* Set zoom level (1.0 = normal, 2.0 = 2x magnification) */
void agentite_camera_set_zoom(Agentite_Camera *cam, float zoom);

/* Set rotation in degrees */
void agentite_camera_set_rotation(Agentite_Camera *cam, float degrees);

/* Update viewport dimensions (call on window resize) */
void agentite_camera_set_viewport(Agentite_Camera *cam, float w, float h);

/* ============================================================================
 * Getters
 * ============================================================================ */

/* Get camera position */
void agentite_camera_get_position(Agentite_Camera *cam, float *x, float *y);

/* Get zoom level */
float agentite_camera_get_zoom(Agentite_Camera *cam);

/* Get rotation in degrees */
float agentite_camera_get_rotation(Agentite_Camera *cam);

/* Get viewport dimensions */
void agentite_camera_get_viewport(Agentite_Camera *cam, float *w, float *h);

/* ============================================================================
 * Matrix Access (for sprite renderer)
 * ============================================================================ */

/* Recompute matrices if dirty (call once per frame before rendering) */
void agentite_camera_update(Agentite_Camera *cam);

/* Get the combined view-projection matrix (16 floats, column-major) */
const float *agentite_camera_get_vp_matrix(Agentite_Camera *cam);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/* Convert screen coordinates to world coordinates (for mouse picking) */
void agentite_camera_screen_to_world(Agentite_Camera *cam,
                                   float screen_x, float screen_y,
                                   float *world_x, float *world_y);

/* Convert world coordinates to screen coordinates */
void agentite_camera_world_to_screen(Agentite_Camera *cam,
                                   float world_x, float world_y,
                                   float *screen_x, float *screen_y);

/* Get visible world bounds (AABB) */
void agentite_camera_get_bounds(Agentite_Camera *cam,
                              float *left, float *right,
                              float *top, float *bottom);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_CAMERA_H */
