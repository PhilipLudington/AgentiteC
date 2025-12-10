/*
 * Carbon 2D Camera Implementation
 */

#include "agentite/agentite.h"
#include "agentite/camera.h"
#include <cglm/cglm.h>
#include <stdlib.h>
#include <math.h>

#define DEG_TO_RAD(d) ((d) * 0.01745329251994329576923690768489f)

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct Agentite_Camera {
    float x, y;              /* Position (world coords of view center) */
    float zoom;              /* 1.0 = normal, 2.0 = 2x magnification */
    float rotation;          /* Radians */
    float viewport_w;
    float viewport_h;

    mat4 view_projection;    /* Combined VP matrix */
    mat4 inverse_vp;         /* For screen->world conversion */
    bool dirty;              /* True if matrices need recomputation */
};

/* ============================================================================
 * Internal: Matrix Computation
 * ============================================================================ */

static void camera_compute_matrices(Agentite_Camera *cam)
{
    if (!cam || !cam->dirty) return;

    /*
     * For 2D camera with Y-down screen coordinates:
     *
     * 1. Orthographic projection maps world units to NDC
     *    - Centered at origin, scaled by zoom
     *    - Y flipped (top = +1, bottom = -1 in NDC)
     *
     * 2. View matrix is inverse of camera transform:
     *    - Camera transform: translate(pos) * rotate(angle)
     *    - View: rotate(-angle) * translate(-pos)
     */

    float half_w = (cam->viewport_w * 0.5f) / cam->zoom;
    float half_h = (cam->viewport_h * 0.5f) / cam->zoom;

    /* Orthographic projection centered at origin */
    mat4 projection;
    glm_ortho(-half_w, half_w,    /* left, right */
              half_h, -half_h,    /* bottom, top (flipped for Y-down) */
              -1.0f, 1.0f,        /* near, far */
              projection);

    /* View matrix: rotate(-rotation) * translate(-position) */
    mat4 view;
    glm_mat4_identity(view);

    /* Apply rotation around Z axis (negative for inverse) */
    if (cam->rotation != 0.0f) {
        glm_rotate_z(view, -cam->rotation, view);
    }

    /* Apply translation (negative for inverse) */
    vec3 translate = {-cam->x, -cam->y, 0.0f};
    glm_translate(view, translate);

    /* Combine: VP = projection * view */
    glm_mat4_mul(projection, view, cam->view_projection);

    /* Compute inverse for screen->world conversion */
    glm_mat4_inv(cam->view_projection, cam->inverse_vp);

    cam->dirty = false;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_Camera *agentite_camera_create(float viewport_w, float viewport_h)
{
    Agentite_Camera *cam = AGENTITE_ALLOC(Agentite_Camera);
    if (!cam) return NULL;

    cam->x = 0.0f;
    cam->y = 0.0f;
    cam->zoom = 1.0f;
    cam->rotation = 0.0f;
    cam->viewport_w = viewport_w;
    cam->viewport_h = viewport_h;
    cam->dirty = true;

    glm_mat4_identity(cam->view_projection);
    glm_mat4_identity(cam->inverse_vp);

    return cam;
}

void agentite_camera_destroy(Agentite_Camera *camera)
{
    free(camera);
}

/* ============================================================================
 * Transform Setters
 * ============================================================================ */

void agentite_camera_set_position(Agentite_Camera *cam, float x, float y)
{
    if (!cam) return;
    cam->x = x;
    cam->y = y;
    cam->dirty = true;
}

void agentite_camera_move(Agentite_Camera *cam, float dx, float dy)
{
    if (!cam) return;
    cam->x += dx;
    cam->y += dy;
    cam->dirty = true;
}

void agentite_camera_set_zoom(Agentite_Camera *cam, float zoom)
{
    if (!cam) return;
    /* Clamp zoom to reasonable range */
    if (zoom < 0.1f) zoom = 0.1f;
    if (zoom > 10.0f) zoom = 10.0f;
    cam->zoom = zoom;
    cam->dirty = true;
}

void agentite_camera_set_rotation(Agentite_Camera *cam, float degrees)
{
    if (!cam) return;
    cam->rotation = DEG_TO_RAD(degrees);
    cam->dirty = true;
}

void agentite_camera_set_viewport(Agentite_Camera *cam, float w, float h)
{
    if (!cam) return;
    cam->viewport_w = w;
    cam->viewport_h = h;
    cam->dirty = true;
}

/* ============================================================================
 * Getters
 * ============================================================================ */

void agentite_camera_get_position(Agentite_Camera *cam, float *x, float *y)
{
    if (!cam) return;
    if (x) *x = cam->x;
    if (y) *y = cam->y;
}

float agentite_camera_get_zoom(Agentite_Camera *cam)
{
    return cam ? cam->zoom : 1.0f;
}

float agentite_camera_get_rotation(Agentite_Camera *cam)
{
    if (!cam) return 0.0f;
    /* Convert back to degrees */
    return cam->rotation / 0.01745329251994329576923690768489f;
}

void agentite_camera_get_viewport(Agentite_Camera *cam, float *w, float *h)
{
    if (!cam) return;
    if (w) *w = cam->viewport_w;
    if (h) *h = cam->viewport_h;
}

/* ============================================================================
 * Matrix Access
 * ============================================================================ */

void agentite_camera_update(Agentite_Camera *cam)
{
    camera_compute_matrices(cam);
}

const float *agentite_camera_get_vp_matrix(Agentite_Camera *cam)
{
    if (!cam) return NULL;
    camera_compute_matrices(cam);
    return (const float *)cam->view_projection;
}

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

void agentite_camera_screen_to_world(Agentite_Camera *cam,
                                   float screen_x, float screen_y,
                                   float *world_x, float *world_y)
{
    if (!cam) {
        if (world_x) *world_x = screen_x;
        if (world_y) *world_y = screen_y;
        return;
    }

    camera_compute_matrices(cam);

    /* Convert screen coords to NDC (-1 to 1) */
    float ndc_x = (screen_x / cam->viewport_w) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (screen_y / cam->viewport_h) * 2.0f;

    /* Transform NDC to world using inverse VP matrix */
    vec4 ndc_pos = {ndc_x, ndc_y, 0.0f, 1.0f};
    vec4 world_pos;
    glm_mat4_mulv(cam->inverse_vp, ndc_pos, world_pos);

    if (world_x) *world_x = world_pos[0];
    if (world_y) *world_y = world_pos[1];
}

void agentite_camera_world_to_screen(Agentite_Camera *cam,
                                   float world_x, float world_y,
                                   float *screen_x, float *screen_y)
{
    if (!cam) {
        if (screen_x) *screen_x = world_x;
        if (screen_y) *screen_y = world_y;
        return;
    }

    camera_compute_matrices(cam);

    /* Transform world to NDC using VP matrix */
    vec4 world_pos = {world_x, world_y, 0.0f, 1.0f};
    vec4 ndc_pos;
    glm_mat4_mulv(cam->view_projection, world_pos, ndc_pos);

    /* Convert NDC to screen coords */
    if (screen_x) *screen_x = (ndc_pos[0] + 1.0f) * 0.5f * cam->viewport_w;
    if (screen_y) *screen_y = (1.0f - ndc_pos[1]) * 0.5f * cam->viewport_h;
}

void agentite_camera_get_bounds(Agentite_Camera *cam,
                              float *left, float *right,
                              float *top, float *bottom)
{
    if (!cam) return;

    /* Calculate visible area based on camera position, zoom, and viewport */
    float half_w = (cam->viewport_w * 0.5f) / cam->zoom;
    float half_h = (cam->viewport_h * 0.5f) / cam->zoom;

    /* If there's rotation, compute AABB of rotated rectangle */
    if (cam->rotation != 0.0f) {
        float cos_r = fabsf(cosf(cam->rotation));
        float sin_r = fabsf(sinf(cam->rotation));
        float new_half_w = half_w * cos_r + half_h * sin_r;
        float new_half_h = half_w * sin_r + half_h * cos_r;
        half_w = new_half_w;
        half_h = new_half_h;
    }

    if (left)   *left   = cam->x - half_w;
    if (right)  *right  = cam->x + half_w;
    if (top)    *top    = cam->y - half_h;
    if (bottom) *bottom = cam->y + half_h;
}
