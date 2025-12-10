# Camera System

2D camera with pan, zoom, and rotation support.

## Quick Start

```c
#include "agentite/camera.h"

// Create camera
Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);

// Connect to sprite renderer
agentite_sprite_set_camera(sprites, camera);

// In game loop
agentite_camera_update(camera);
```

## Position & Movement

```c
agentite_camera_set_position(camera, 500.0f, 300.0f);  // Center on world position
agentite_camera_move(camera, dx, dy);                   // Pan by delta
```

## Zoom & Rotation

```c
agentite_camera_set_zoom(camera, 2.0f);      // 2x magnification
agentite_camera_set_rotation(camera, 45.0f); // Rotate 45 degrees
```

## Coordinate Conversion

```c
// Screen to world (for mouse picking)
float world_x, world_y;
agentite_camera_screen_to_world(camera, mouse_x, mouse_y, &world_x, &world_y);

// Get visible world bounds
float left, right, top, bottom;
agentite_camera_get_bounds(camera, &left, &right, &top, &bottom);
```

## Screen-Space Rendering

```c
// For UI elements that shouldn't move with camera
agentite_sprite_set_camera(sr, NULL);
agentite_sprite_begin(sr, NULL);
// ... draw UI ...
```

## Notes

- Call `agentite_camera_update(camera)` each frame to recompute matrices
- When no camera is set, sprites render in screen-space coordinates

---

# 3D Camera System

Orbital 3D camera with spherical coordinate control and smooth animations.

## Quick Start

```c
#include "agentite/camera3d.h"

Agentite_Camera3D *cam = agentite_camera3d_create();
agentite_camera3d_set_perspective(cam, 60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
agentite_camera3d_set_target(cam, 0, 0, 0);
agentite_camera3d_set_spherical(cam, 45.0f, 30.0f, 15.0f);  // yaw, pitch, distance

// In game loop
agentite_camera3d_update(cam, delta_time);
```

## Orbital Controls

```c
agentite_camera3d_orbit(cam, delta_yaw, delta_pitch);  // Rotate around target
agentite_camera3d_zoom(cam, delta_distance);           // Zoom in/out
agentite_camera3d_pan(cam, right, up);                 // Pan in camera space
agentite_camera3d_pan_xz(cam, dx, dz);                 // Pan in world XZ plane
```

## Constraints

```c
agentite_camera3d_set_distance_limits(cam, 5.0f, 100.0f);
agentite_camera3d_set_pitch_limits(cam, -80.0f, 80.0f);
```

## Smooth Animations

```c
agentite_camera3d_animate_spherical_to(cam, 90.0f, 45.0f, 20.0f, 1.5f);
agentite_camera3d_animate_target_to(cam, 10.0f, 0.0f, 10.0f, 1.0f);
agentite_camera3d_stop_animation(cam);
```

## 3D Picking

```c
float ray_ox, ray_oy, ray_oz, ray_dx, ray_dy, ray_dz;
agentite_camera3d_screen_to_ray(cam, mouse_x, mouse_y, screen_w, screen_h,
                               &ray_ox, &ray_oy, &ray_oz,
                               &ray_dx, &ray_dy, &ray_dz);
```

## Get Matrices

```c
const float *view = agentite_camera3d_get_view_matrix(cam);
const float *proj = agentite_camera3d_get_projection_matrix(cam);
const float *vp = agentite_camera3d_get_vp_matrix(cam);
```
