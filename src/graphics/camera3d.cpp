/*
 * Carbon 3D Camera Implementation
 *
 * Orbital camera with spherical coordinate control and smooth animations.
 */

#include "agentite/agentite.h"
#include "agentite/camera3d.h"
#include <cglm/cglm.h>
#include <stdlib.h>
#include <math.h>

#define DEG_TO_RAD(d) ((d) * 0.01745329251994329576923690768489f)
#define RAD_TO_DEG(r) ((r) * 57.295779513082320876798154814105f)

/* Easing types */
#define EASING_LINEAR    0
#define EASING_SMOOTH    1  /* Smoothstep (ease-in-out) */

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct Agentite_Camera3D {
    /* Position and target */
    vec3 position;
    vec3 target;

    /* Spherical coordinates (stored in radians internally) */
    float yaw;          /* Horizontal angle */
    float pitch;        /* Vertical angle */
    float distance;     /* Distance from target */

    /* Constraints */
    float min_distance, max_distance;
    float min_pitch, max_pitch;   /* In radians */

    /* Projection settings */
    Agentite_ProjectionType projection_type;
    float fov;          /* Perspective FOV in radians */
    float aspect;       /* Width / height */
    float near_plane, far_plane;
    float ortho_width, ortho_height;

    /* Matrices */
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    bool dirty;

    /* Animation state */
    bool animating;
    int easing_type;
    float anim_time;
    float anim_duration;

    /* Animation mode: 0=position, 1=spherical, 2=target */
    int anim_mode;

    /* Start values for animation */
    vec3 anim_start_pos;
    vec3 anim_start_target;
    float anim_start_yaw, anim_start_pitch, anim_start_distance;

    /* End values for animation */
    vec3 anim_end_pos;
    vec3 anim_end_target;
    float anim_end_yaw, anim_end_pitch, anim_end_distance;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/* Smoothstep easing function */
static float ease_smoothstep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

/* Apply easing based on type */
static float apply_easing(int type, float t)
{
    switch (type) {
        case EASING_SMOOTH:
            return ease_smoothstep(t);
        default:
            return t;  /* Linear */
    }
}

/* Clamp float to range */
static float clampf(float val, float min, float max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

/* Update position from spherical coordinates */
static void update_position_from_spherical(Agentite_Camera3D *cam)
{
    /* Convert spherical to Cartesian
     * x = target.x + distance * cos(pitch) * cos(yaw)
     * y = target.y + distance * sin(pitch)
     * z = target.z + distance * cos(pitch) * sin(yaw)
     */
    float cos_pitch = cosf(cam->pitch);
    float sin_pitch = sinf(cam->pitch);
    float cos_yaw = cosf(cam->yaw);
    float sin_yaw = sinf(cam->yaw);

    cam->position[0] = cam->target[0] + cam->distance * cos_pitch * cos_yaw;
    cam->position[1] = cam->target[1] + cam->distance * sin_pitch;
    cam->position[2] = cam->target[2] + cam->distance * cos_pitch * sin_yaw;

    cam->dirty = true;
}

/* Update spherical from position (for direct position setting) */
static void update_spherical_from_position(Agentite_Camera3D *cam)
{
    vec3 dir;
    glm_vec3_sub(cam->position, cam->target, dir);

    cam->distance = glm_vec3_norm(dir);
    if (cam->distance < 0.001f) {
        cam->distance = 0.001f;
        return;
    }

    /* Normalize */
    glm_vec3_scale(dir, 1.0f / cam->distance, dir);

    /* Extract angles */
    cam->pitch = asinf(clampf(dir[1], -1.0f, 1.0f));
    cam->yaw = atan2f(dir[2], dir[0]);
}

/* Compute matrices */
static void compute_matrices(Agentite_Camera3D *cam)
{
    if (!cam || !cam->dirty) return;

    /* View matrix: look at target from position */
    vec3 up = {0.0f, 1.0f, 0.0f};
    glm_lookat(cam->position, cam->target, up, cam->view);

    /* Projection matrix */
    if (cam->projection_type == AGENTITE_PROJECTION_PERSPECTIVE) {
        glm_perspective(cam->fov, cam->aspect, cam->near_plane, cam->far_plane,
                        cam->projection);
    } else {
        float half_w = cam->ortho_width * 0.5f;
        float half_h = cam->ortho_height * 0.5f;
        glm_ortho(-half_w, half_w, -half_h, half_h,
                  cam->near_plane, cam->far_plane, cam->projection);
    }

    /* Combined VP */
    glm_mat4_mul(cam->projection, cam->view, cam->view_projection);

    cam->dirty = false;
}

/* Apply constraints */
static void apply_constraints(Agentite_Camera3D *cam)
{
    /* Pitch limits */
    cam->pitch = clampf(cam->pitch, cam->min_pitch, cam->max_pitch);

    /* Distance limits */
    if (cam->min_distance > 0 && cam->distance < cam->min_distance) {
        cam->distance = cam->min_distance;
    }
    if (cam->max_distance > 0 && cam->distance > cam->max_distance) {
        cam->distance = cam->max_distance;
    }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_Camera3D *agentite_camera3d_create(void)
{
    Agentite_Camera3D *cam = AGENTITE_ALLOC(Agentite_Camera3D);
    if (!cam) return NULL;

    /* Default position: looking at origin from (0, 5, 10) */
    glm_vec3_zero(cam->target);
    cam->yaw = 0.0f;
    cam->pitch = DEG_TO_RAD(30.0f);
    cam->distance = 10.0f;
    update_position_from_spherical(cam);

    /* Default constraints */
    cam->min_distance = 1.0f;
    cam->max_distance = 1000.0f;
    cam->min_pitch = DEG_TO_RAD(-89.0f);
    cam->max_pitch = DEG_TO_RAD(89.0f);

    /* Default perspective projection */
    cam->projection_type = AGENTITE_PROJECTION_PERSPECTIVE;
    cam->fov = DEG_TO_RAD(60.0f);
    cam->aspect = 16.0f / 9.0f;
    cam->near_plane = 0.1f;
    cam->far_plane = 1000.0f;
    cam->ortho_width = 20.0f;
    cam->ortho_height = 20.0f;

    /* Animation */
    cam->animating = false;
    cam->easing_type = EASING_SMOOTH;

    cam->dirty = true;

    glm_mat4_identity(cam->view);
    glm_mat4_identity(cam->projection);
    glm_mat4_identity(cam->view_projection);

    return cam;
}

void agentite_camera3d_destroy(Agentite_Camera3D *cam)
{
    free(cam);
}

/* ============================================================================
 * Position (Cartesian)
 * ============================================================================ */

void agentite_camera3d_set_position(Agentite_Camera3D *cam, float x, float y, float z)
{
    if (!cam) return;
    cam->position[0] = x;
    cam->position[1] = y;
    cam->position[2] = z;
    update_spherical_from_position(cam);
    cam->dirty = true;
}

void agentite_camera3d_get_position(Agentite_Camera3D *cam, float *x, float *y, float *z)
{
    if (!cam) return;
    if (x) *x = cam->position[0];
    if (y) *y = cam->position[1];
    if (z) *z = cam->position[2];
}

void agentite_camera3d_set_target(Agentite_Camera3D *cam, float x, float y, float z)
{
    if (!cam) return;
    cam->target[0] = x;
    cam->target[1] = y;
    cam->target[2] = z;
    update_position_from_spherical(cam);
}

void agentite_camera3d_get_target(Agentite_Camera3D *cam, float *x, float *y, float *z)
{
    if (!cam) return;
    if (x) *x = cam->target[0];
    if (y) *y = cam->target[1];
    if (z) *z = cam->target[2];
}

/* ============================================================================
 * Spherical Coordinates
 * ============================================================================ */

void agentite_camera3d_set_spherical(Agentite_Camera3D *cam,
                                    float yaw, float pitch, float distance)
{
    if (!cam) return;
    cam->yaw = DEG_TO_RAD(yaw);
    cam->pitch = DEG_TO_RAD(pitch);
    cam->distance = distance;
    apply_constraints(cam);
    update_position_from_spherical(cam);
}

void agentite_camera3d_get_spherical(Agentite_Camera3D *cam,
                                    float *yaw, float *pitch, float *distance)
{
    if (!cam) return;
    if (yaw) *yaw = RAD_TO_DEG(cam->yaw);
    if (pitch) *pitch = RAD_TO_DEG(cam->pitch);
    if (distance) *distance = cam->distance;
}

/* ============================================================================
 * Orbital Controls
 * ============================================================================ */

void agentite_camera3d_orbit(Agentite_Camera3D *cam, float delta_yaw, float delta_pitch)
{
    if (!cam) return;
    cam->yaw += DEG_TO_RAD(delta_yaw);
    cam->pitch += DEG_TO_RAD(delta_pitch);
    apply_constraints(cam);
    update_position_from_spherical(cam);
}

void agentite_camera3d_zoom(Agentite_Camera3D *cam, float delta)
{
    if (!cam) return;
    cam->distance += delta;
    apply_constraints(cam);
    update_position_from_spherical(cam);
}

void agentite_camera3d_pan(Agentite_Camera3D *cam, float right, float up)
{
    if (!cam) return;

    /* Get camera right and up vectors */
    vec3 cam_right, cam_up;

    /* Right vector from view matrix row 0 */
    cam_right[0] = cam->view[0][0];
    cam_right[1] = cam->view[1][0];
    cam_right[2] = cam->view[2][0];

    /* Up vector from view matrix row 1 */
    cam_up[0] = cam->view[0][1];
    cam_up[1] = cam->view[1][1];
    cam_up[2] = cam->view[2][1];

    /* Move target */
    vec3 delta;
    glm_vec3_scale(cam_right, right, delta);
    glm_vec3_add(cam->target, delta, cam->target);
    glm_vec3_scale(cam_up, up, delta);
    glm_vec3_add(cam->target, delta, cam->target);

    update_position_from_spherical(cam);
}

void agentite_camera3d_pan_xz(Agentite_Camera3D *cam, float dx, float dz)
{
    if (!cam) return;
    cam->target[0] += dx;
    cam->target[2] += dz;
    update_position_from_spherical(cam);
}

/* ============================================================================
 * Constraints
 * ============================================================================ */

void agentite_camera3d_set_distance_limits(Agentite_Camera3D *cam, float min, float max)
{
    if (!cam) return;
    cam->min_distance = min;
    cam->max_distance = max;
    apply_constraints(cam);
    update_position_from_spherical(cam);
}

void agentite_camera3d_set_pitch_limits(Agentite_Camera3D *cam, float min, float max)
{
    if (!cam) return;
    cam->min_pitch = DEG_TO_RAD(min);
    cam->max_pitch = DEG_TO_RAD(max);
    apply_constraints(cam);
    update_position_from_spherical(cam);
}

void agentite_camera3d_get_distance_limits(Agentite_Camera3D *cam, float *min, float *max)
{
    if (!cam) return;
    if (min) *min = cam->min_distance;
    if (max) *max = cam->max_distance;
}

void agentite_camera3d_get_pitch_limits(Agentite_Camera3D *cam, float *min, float *max)
{
    if (!cam) return;
    if (min) *min = RAD_TO_DEG(cam->min_pitch);
    if (max) *max = RAD_TO_DEG(cam->max_pitch);
}

/* ============================================================================
 * Projection
 * ============================================================================ */

void agentite_camera3d_set_perspective(Agentite_Camera3D *cam,
                                      float fov, float aspect,
                                      float near, float far)
{
    if (!cam) return;
    cam->projection_type = AGENTITE_PROJECTION_PERSPECTIVE;
    cam->fov = DEG_TO_RAD(fov);
    cam->aspect = aspect;
    cam->near_plane = near;
    cam->far_plane = far;
    cam->dirty = true;
}

void agentite_camera3d_set_orthographic(Agentite_Camera3D *cam,
                                       float width, float height,
                                       float near, float far)
{
    if (!cam) return;
    cam->projection_type = AGENTITE_PROJECTION_ORTHOGRAPHIC;
    cam->ortho_width = width;
    cam->ortho_height = height;
    cam->near_plane = near;
    cam->far_plane = far;
    cam->dirty = true;
}

Agentite_ProjectionType agentite_camera3d_get_projection_type(Agentite_Camera3D *cam)
{
    return cam ? cam->projection_type : AGENTITE_PROJECTION_PERSPECTIVE;
}

void agentite_camera3d_set_aspect(Agentite_Camera3D *cam, float aspect)
{
    if (!cam) return;
    cam->aspect = aspect;
    /* Update ortho dimensions maintaining same height */
    cam->ortho_width = cam->ortho_height * aspect;
    cam->dirty = true;
}

/* ============================================================================
 * Matrix Access
 * ============================================================================ */

void agentite_camera3d_update(Agentite_Camera3D *cam, float delta_time)
{
    if (!cam) return;

    /* Process animation */
    if (cam->animating) {
        cam->anim_time += delta_time;
        float t = cam->anim_time / cam->anim_duration;

        if (t >= 1.0f) {
            t = 1.0f;
            cam->animating = false;
        }

        float eased = apply_easing(cam->easing_type, t);

        switch (cam->anim_mode) {
            case 0:  /* Position animation */
                glm_vec3_lerp(cam->anim_start_pos, cam->anim_end_pos, eased, cam->position);
                update_spherical_from_position(cam);
                break;

            case 1:  /* Spherical animation */
                cam->yaw = cam->anim_start_yaw + (cam->anim_end_yaw - cam->anim_start_yaw) * eased;
                cam->pitch = cam->anim_start_pitch + (cam->anim_end_pitch - cam->anim_start_pitch) * eased;
                cam->distance = cam->anim_start_distance + (cam->anim_end_distance - cam->anim_start_distance) * eased;
                update_position_from_spherical(cam);
                break;

            case 2:  /* Target animation */
                glm_vec3_lerp(cam->anim_start_target, cam->anim_end_target, eased, cam->target);
                update_position_from_spherical(cam);
                break;
        }

        cam->dirty = true;
    }

    compute_matrices(cam);
}

const float *agentite_camera3d_get_view_matrix(Agentite_Camera3D *cam)
{
    if (!cam) return NULL;
    compute_matrices(cam);
    return (const float *)cam->view;
}

const float *agentite_camera3d_get_projection_matrix(Agentite_Camera3D *cam)
{
    if (!cam) return NULL;
    compute_matrices(cam);
    return (const float *)cam->projection;
}

const float *agentite_camera3d_get_vp_matrix(Agentite_Camera3D *cam)
{
    if (!cam) return NULL;
    compute_matrices(cam);
    return (const float *)cam->view_projection;
}

/* ============================================================================
 * Direction Vectors
 * ============================================================================ */

void agentite_camera3d_get_forward(Agentite_Camera3D *cam, float *x, float *y, float *z)
{
    if (!cam) return;
    compute_matrices(cam);

    /* Forward is negative Z in view space, row 2 of view matrix */
    if (x) *x = -cam->view[0][2];
    if (y) *y = -cam->view[1][2];
    if (z) *z = -cam->view[2][2];
}

void agentite_camera3d_get_right(Agentite_Camera3D *cam, float *x, float *y, float *z)
{
    if (!cam) return;
    compute_matrices(cam);

    /* Right is row 0 of view matrix */
    if (x) *x = cam->view[0][0];
    if (y) *y = cam->view[1][0];
    if (z) *z = cam->view[2][0];
}

void agentite_camera3d_get_up(Agentite_Camera3D *cam, float *x, float *y, float *z)
{
    if (!cam) return;
    compute_matrices(cam);

    /* Up is row 1 of view matrix */
    if (x) *x = cam->view[0][1];
    if (y) *y = cam->view[1][1];
    if (z) *z = cam->view[2][1];
}

/* ============================================================================
 * Smooth Transitions
 * ============================================================================ */

void agentite_camera3d_animate_to(Agentite_Camera3D *cam,
                                 float x, float y, float z,
                                 float duration)
{
    if (!cam || duration <= 0) {
        if (cam) agentite_camera3d_set_position(cam, x, y, z);
        return;
    }

    glm_vec3_copy(cam->position, cam->anim_start_pos);
    cam->anim_end_pos[0] = x;
    cam->anim_end_pos[1] = y;
    cam->anim_end_pos[2] = z;

    cam->anim_mode = 0;
    cam->anim_time = 0;
    cam->anim_duration = duration;
    cam->animating = true;
}

void agentite_camera3d_animate_spherical_to(Agentite_Camera3D *cam,
                                           float yaw, float pitch, float distance,
                                           float duration)
{
    if (!cam || duration <= 0) {
        if (cam) agentite_camera3d_set_spherical(cam, yaw, pitch, distance);
        return;
    }

    cam->anim_start_yaw = cam->yaw;
    cam->anim_start_pitch = cam->pitch;
    cam->anim_start_distance = cam->distance;

    cam->anim_end_yaw = DEG_TO_RAD(yaw);
    cam->anim_end_pitch = DEG_TO_RAD(pitch);
    cam->anim_end_distance = distance;

    cam->anim_mode = 1;
    cam->anim_time = 0;
    cam->anim_duration = duration;
    cam->animating = true;
}

void agentite_camera3d_animate_target_to(Agentite_Camera3D *cam,
                                        float x, float y, float z,
                                        float duration)
{
    if (!cam || duration <= 0) {
        if (cam) agentite_camera3d_set_target(cam, x, y, z);
        return;
    }

    glm_vec3_copy(cam->target, cam->anim_start_target);
    cam->anim_end_target[0] = x;
    cam->anim_end_target[1] = y;
    cam->anim_end_target[2] = z;

    cam->anim_mode = 2;
    cam->anim_time = 0;
    cam->anim_duration = duration;
    cam->animating = true;
}

bool agentite_camera3d_is_animating(Agentite_Camera3D *cam)
{
    return cam ? cam->animating : false;
}

void agentite_camera3d_stop_animation(Agentite_Camera3D *cam)
{
    if (cam) cam->animating = false;
}

void agentite_camera3d_set_easing(Agentite_Camera3D *cam, int easing_type)
{
    if (cam) cam->easing_type = easing_type;
}

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

void agentite_camera3d_screen_to_ray(Agentite_Camera3D *cam,
                                    float screen_x, float screen_y,
                                    float screen_w, float screen_h,
                                    float *ray_origin_x, float *ray_origin_y, float *ray_origin_z,
                                    float *ray_dir_x, float *ray_dir_y, float *ray_dir_z)
{
    if (!cam) return;

    compute_matrices(cam);

    /* Convert screen to NDC */
    float ndc_x = (screen_x / screen_w) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (screen_y / screen_h) * 2.0f;

    /* Inverse VP matrix */
    mat4 inv_vp;
    glm_mat4_inv(cam->view_projection, inv_vp);

    /* Near plane point */
    vec4 near_ndc = {ndc_x, ndc_y, -1.0f, 1.0f};
    vec4 near_world;
    glm_mat4_mulv(inv_vp, near_ndc, near_world);
    glm_vec4_scale(near_world, 1.0f / near_world[3], near_world);

    /* Far plane point */
    vec4 far_ndc = {ndc_x, ndc_y, 1.0f, 1.0f};
    vec4 far_world;
    glm_mat4_mulv(inv_vp, far_ndc, far_world);
    glm_vec4_scale(far_world, 1.0f / far_world[3], far_world);

    /* Ray origin is near point */
    if (ray_origin_x) *ray_origin_x = near_world[0];
    if (ray_origin_y) *ray_origin_y = near_world[1];
    if (ray_origin_z) *ray_origin_z = near_world[2];

    /* Ray direction is normalized (far - near) */
    vec3 dir;
    dir[0] = far_world[0] - near_world[0];
    dir[1] = far_world[1] - near_world[1];
    dir[2] = far_world[2] - near_world[2];
    glm_vec3_normalize(dir);

    if (ray_dir_x) *ray_dir_x = dir[0];
    if (ray_dir_y) *ray_dir_y = dir[1];
    if (ray_dir_z) *ray_dir_z = dir[2];
}

bool agentite_camera3d_world_to_screen(Agentite_Camera3D *cam,
                                      float world_x, float world_y, float world_z,
                                      float screen_w, float screen_h,
                                      float *screen_x, float *screen_y)
{
    if (!cam) return false;

    compute_matrices(cam);

    /* Transform to clip space */
    vec4 world_pos = {world_x, world_y, world_z, 1.0f};
    vec4 clip_pos;
    glm_mat4_mulv(cam->view_projection, world_pos, clip_pos);

    /* Check if behind camera */
    if (clip_pos[3] <= 0) return false;

    /* Perspective divide to NDC */
    float ndc_x = clip_pos[0] / clip_pos[3];
    float ndc_y = clip_pos[1] / clip_pos[3];

    /* Convert NDC to screen */
    if (screen_x) *screen_x = (ndc_x + 1.0f) * 0.5f * screen_w;
    if (screen_y) *screen_y = (1.0f - ndc_y) * 0.5f * screen_h;

    return true;
}
