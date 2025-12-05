/*
 * Carbon 3D Camera System
 *
 * Orbital camera for 3D views (galaxy maps, isometric, etc.) with:
 * - Spherical coordinate positioning (yaw, pitch, distance)
 * - Target-based orbiting
 * - Perspective and orthographic projection
 * - Smooth animated transitions
 * - Constraint limits for pitch and distance
 */

#ifndef CARBON_CAMERA3D_H
#define CARBON_CAMERA3D_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque camera handle */
typedef struct Carbon_Camera3D Carbon_Camera3D;

/* Projection type */
typedef enum {
    CARBON_PROJECTION_PERSPECTIVE,
    CARBON_PROJECTION_ORTHOGRAPHIC
} Carbon_ProjectionType;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/* Create 3D camera */
Carbon_Camera3D *carbon_camera3d_create(void);

/* Destroy camera */
void carbon_camera3d_destroy(Carbon_Camera3D *cam);

/* ============================================================================
 * Position (Cartesian)
 * ============================================================================ */

/* Set camera position directly in world coordinates */
void carbon_camera3d_set_position(Carbon_Camera3D *cam, float x, float y, float z);

/* Get camera position */
void carbon_camera3d_get_position(Carbon_Camera3D *cam, float *x, float *y, float *z);

/* Set the target point the camera looks at */
void carbon_camera3d_set_target(Carbon_Camera3D *cam, float x, float y, float z);

/* Get target position */
void carbon_camera3d_get_target(Carbon_Camera3D *cam, float *x, float *y, float *z);

/* ============================================================================
 * Spherical Coordinates (Orbital)
 * ============================================================================ */

/* Set camera using spherical coordinates around target
 * yaw: horizontal angle in degrees (0 = +X axis, 90 = +Z axis)
 * pitch: vertical angle in degrees (0 = horizontal, 90 = straight down)
 * distance: distance from target */
void carbon_camera3d_set_spherical(Carbon_Camera3D *cam,
                                    float yaw, float pitch, float distance);

/* Get spherical coordinates */
void carbon_camera3d_get_spherical(Carbon_Camera3D *cam,
                                    float *yaw, float *pitch, float *distance);

/* ============================================================================
 * Orbital Controls
 * ============================================================================ */

/* Orbit around target by delta angles (degrees) */
void carbon_camera3d_orbit(Carbon_Camera3D *cam, float delta_yaw, float delta_pitch);

/* Zoom (change distance to target) */
void carbon_camera3d_zoom(Carbon_Camera3D *cam, float delta);

/* Pan (move target and camera together) in camera-relative directions */
void carbon_camera3d_pan(Carbon_Camera3D *cam, float right, float up);

/* Pan in world XZ plane (useful for strategy games) */
void carbon_camera3d_pan_xz(Carbon_Camera3D *cam, float dx, float dz);

/* ============================================================================
 * Constraints
 * ============================================================================ */

/* Set distance limits (min, max) - 0 for unlimited */
void carbon_camera3d_set_distance_limits(Carbon_Camera3D *cam, float min, float max);

/* Set pitch limits in degrees (e.g., -89 to 89 to prevent gimbal lock) */
void carbon_camera3d_set_pitch_limits(Carbon_Camera3D *cam, float min, float max);

/* Get current limits */
void carbon_camera3d_get_distance_limits(Carbon_Camera3D *cam, float *min, float *max);
void carbon_camera3d_get_pitch_limits(Carbon_Camera3D *cam, float *min, float *max);

/* ============================================================================
 * Projection
 * ============================================================================ */

/* Set perspective projection
 * fov: field of view in degrees (vertical)
 * aspect: width/height ratio
 * near/far: clipping planes */
void carbon_camera3d_set_perspective(Carbon_Camera3D *cam,
                                      float fov, float aspect,
                                      float near, float far);

/* Set orthographic projection
 * width/height: visible area size
 * near/far: clipping planes */
void carbon_camera3d_set_orthographic(Carbon_Camera3D *cam,
                                       float width, float height,
                                       float near, float far);

/* Get projection type */
Carbon_ProjectionType carbon_camera3d_get_projection_type(Carbon_Camera3D *cam);

/* Update aspect ratio (call on window resize) */
void carbon_camera3d_set_aspect(Carbon_Camera3D *cam, float aspect);

/* ============================================================================
 * Matrix Access
 * ============================================================================ */

/* Update camera matrices (call once per frame) */
void carbon_camera3d_update(Carbon_Camera3D *cam, float delta_time);

/* Get view matrix (16 floats, column-major) */
const float *carbon_camera3d_get_view_matrix(Carbon_Camera3D *cam);

/* Get projection matrix (16 floats, column-major) */
const float *carbon_camera3d_get_projection_matrix(Carbon_Camera3D *cam);

/* Get combined view-projection matrix (16 floats, column-major) */
const float *carbon_camera3d_get_vp_matrix(Carbon_Camera3D *cam);

/* ============================================================================
 * Direction Vectors
 * ============================================================================ */

/* Get camera forward direction (normalized) */
void carbon_camera3d_get_forward(Carbon_Camera3D *cam, float *x, float *y, float *z);

/* Get camera right direction (normalized) */
void carbon_camera3d_get_right(Carbon_Camera3D *cam, float *x, float *y, float *z);

/* Get camera up direction (normalized) */
void carbon_camera3d_get_up(Carbon_Camera3D *cam, float *x, float *y, float *z);

/* ============================================================================
 * Smooth Transitions (Animation)
 * ============================================================================ */

/* Animate camera position to target position over duration seconds */
void carbon_camera3d_animate_to(Carbon_Camera3D *cam,
                                 float x, float y, float z,
                                 float duration);

/* Animate spherical coordinates over duration seconds */
void carbon_camera3d_animate_spherical_to(Carbon_Camera3D *cam,
                                           float yaw, float pitch, float distance,
                                           float duration);

/* Animate target position over duration seconds */
void carbon_camera3d_animate_target_to(Carbon_Camera3D *cam,
                                        float x, float y, float z,
                                        float duration);

/* Check if camera is currently animating */
bool carbon_camera3d_is_animating(Carbon_Camera3D *cam);

/* Stop any active animation */
void carbon_camera3d_stop_animation(Carbon_Camera3D *cam);

/* Set animation easing (0 = linear, 1 = ease-in-out, default) */
void carbon_camera3d_set_easing(Carbon_Camera3D *cam, int easing_type);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/* Convert screen coordinates to world ray (for 3D picking)
 * Returns ray origin and direction */
void carbon_camera3d_screen_to_ray(Carbon_Camera3D *cam,
                                    float screen_x, float screen_y,
                                    float screen_w, float screen_h,
                                    float *ray_origin_x, float *ray_origin_y, float *ray_origin_z,
                                    float *ray_dir_x, float *ray_dir_y, float *ray_dir_z);

/* Project world point to screen coordinates
 * Returns false if point is behind camera */
bool carbon_camera3d_world_to_screen(Carbon_Camera3D *cam,
                                      float world_x, float world_y, float world_z,
                                      float screen_w, float screen_h,
                                      float *screen_x, float *screen_y);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_CAMERA3D_H */
