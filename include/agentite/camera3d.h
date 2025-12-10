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

#ifndef AGENTITE_CAMERA3D_H
#define AGENTITE_CAMERA3D_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque camera handle */
typedef struct Agentite_Camera3D Agentite_Camera3D;

/* Projection type */
typedef enum {
    AGENTITE_PROJECTION_PERSPECTIVE,
    AGENTITE_PROJECTION_ORTHOGRAPHIC
} Agentite_ProjectionType;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/* Create 3D camera */
Agentite_Camera3D *agentite_camera3d_create(void);

/* Destroy camera */
void agentite_camera3d_destroy(Agentite_Camera3D *cam);

/* ============================================================================
 * Position (Cartesian)
 * ============================================================================ */

/* Set camera position directly in world coordinates */
void agentite_camera3d_set_position(Agentite_Camera3D *cam, float x, float y, float z);

/* Get camera position */
void agentite_camera3d_get_position(Agentite_Camera3D *cam, float *x, float *y, float *z);

/* Set the target point the camera looks at */
void agentite_camera3d_set_target(Agentite_Camera3D *cam, float x, float y, float z);

/* Get target position */
void agentite_camera3d_get_target(Agentite_Camera3D *cam, float *x, float *y, float *z);

/* ============================================================================
 * Spherical Coordinates (Orbital)
 * ============================================================================ */

/* Set camera using spherical coordinates around target
 * yaw: horizontal angle in degrees (0 = +X axis, 90 = +Z axis)
 * pitch: vertical angle in degrees (0 = horizontal, 90 = straight down)
 * distance: distance from target */
void agentite_camera3d_set_spherical(Agentite_Camera3D *cam,
                                    float yaw, float pitch, float distance);

/* Get spherical coordinates */
void agentite_camera3d_get_spherical(Agentite_Camera3D *cam,
                                    float *yaw, float *pitch, float *distance);

/* ============================================================================
 * Orbital Controls
 * ============================================================================ */

/* Orbit around target by delta angles (degrees) */
void agentite_camera3d_orbit(Agentite_Camera3D *cam, float delta_yaw, float delta_pitch);

/* Zoom (change distance to target) */
void agentite_camera3d_zoom(Agentite_Camera3D *cam, float delta);

/* Pan (move target and camera together) in camera-relative directions */
void agentite_camera3d_pan(Agentite_Camera3D *cam, float right, float up);

/* Pan in world XZ plane (useful for strategy games) */
void agentite_camera3d_pan_xz(Agentite_Camera3D *cam, float dx, float dz);

/* ============================================================================
 * Constraints
 * ============================================================================ */

/* Set distance limits (min, max) - 0 for unlimited */
void agentite_camera3d_set_distance_limits(Agentite_Camera3D *cam, float min, float max);

/* Set pitch limits in degrees (e.g., -89 to 89 to prevent gimbal lock) */
void agentite_camera3d_set_pitch_limits(Agentite_Camera3D *cam, float min, float max);

/* Get current limits */
void agentite_camera3d_get_distance_limits(Agentite_Camera3D *cam, float *min, float *max);
void agentite_camera3d_get_pitch_limits(Agentite_Camera3D *cam, float *min, float *max);

/* ============================================================================
 * Projection
 * ============================================================================ */

/* Set perspective projection
 * fov: field of view in degrees (vertical)
 * aspect: width/height ratio
 * near/far: clipping planes */
void agentite_camera3d_set_perspective(Agentite_Camera3D *cam,
                                      float fov, float aspect,
                                      float near, float far);

/* Set orthographic projection
 * width/height: visible area size
 * near/far: clipping planes */
void agentite_camera3d_set_orthographic(Agentite_Camera3D *cam,
                                       float width, float height,
                                       float near, float far);

/* Get projection type */
Agentite_ProjectionType agentite_camera3d_get_projection_type(Agentite_Camera3D *cam);

/* Update aspect ratio (call on window resize) */
void agentite_camera3d_set_aspect(Agentite_Camera3D *cam, float aspect);

/* ============================================================================
 * Matrix Access
 * ============================================================================ */

/* Update camera matrices (call once per frame) */
void agentite_camera3d_update(Agentite_Camera3D *cam, float delta_time);

/* Get view matrix (16 floats, column-major) */
const float *agentite_camera3d_get_view_matrix(Agentite_Camera3D *cam);

/* Get projection matrix (16 floats, column-major) */
const float *agentite_camera3d_get_projection_matrix(Agentite_Camera3D *cam);

/* Get combined view-projection matrix (16 floats, column-major) */
const float *agentite_camera3d_get_vp_matrix(Agentite_Camera3D *cam);

/* ============================================================================
 * Direction Vectors
 * ============================================================================ */

/* Get camera forward direction (normalized) */
void agentite_camera3d_get_forward(Agentite_Camera3D *cam, float *x, float *y, float *z);

/* Get camera right direction (normalized) */
void agentite_camera3d_get_right(Agentite_Camera3D *cam, float *x, float *y, float *z);

/* Get camera up direction (normalized) */
void agentite_camera3d_get_up(Agentite_Camera3D *cam, float *x, float *y, float *z);

/* ============================================================================
 * Smooth Transitions (Animation)
 * ============================================================================ */

/* Animate camera position to target position over duration seconds */
void agentite_camera3d_animate_to(Agentite_Camera3D *cam,
                                 float x, float y, float z,
                                 float duration);

/* Animate spherical coordinates over duration seconds */
void agentite_camera3d_animate_spherical_to(Agentite_Camera3D *cam,
                                           float yaw, float pitch, float distance,
                                           float duration);

/* Animate target position over duration seconds */
void agentite_camera3d_animate_target_to(Agentite_Camera3D *cam,
                                        float x, float y, float z,
                                        float duration);

/* Check if camera is currently animating */
bool agentite_camera3d_is_animating(Agentite_Camera3D *cam);

/* Stop any active animation */
void agentite_camera3d_stop_animation(Agentite_Camera3D *cam);

/* Set animation easing (0 = linear, 1 = ease-in-out, default) */
void agentite_camera3d_set_easing(Agentite_Camera3D *cam, int easing_type);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/* Convert screen coordinates to world ray (for 3D picking)
 * Returns ray origin and direction */
void agentite_camera3d_screen_to_ray(Agentite_Camera3D *cam,
                                    float screen_x, float screen_y,
                                    float screen_w, float screen_h,
                                    float *ray_origin_x, float *ray_origin_y, float *ray_origin_z,
                                    float *ray_dir_x, float *ray_dir_y, float *ray_dir_z);

/* Project world point to screen coordinates
 * Returns false if point is behind camera */
bool agentite_camera3d_world_to_screen(Agentite_Camera3D *cam,
                                      float world_x, float world_y, float world_z,
                                      float screen_w, float screen_h,
                                      float *screen_x, float *screen_y);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_CAMERA3D_H */
