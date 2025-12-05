# Camera System

2D camera with pan, zoom, and rotation support.

## Quick Start

```c
#include "carbon/camera.h"

// Create camera
Carbon_Camera *camera = carbon_camera_create(1280.0f, 720.0f);

// Connect to sprite renderer
carbon_sprite_set_camera(sprites, camera);

// In game loop
carbon_camera_update(camera);
```

## Position & Movement

```c
carbon_camera_set_position(camera, 500.0f, 300.0f);  // Center on world position
carbon_camera_move(camera, dx, dy);                   // Pan by delta
```

## Zoom & Rotation

```c
carbon_camera_set_zoom(camera, 2.0f);      // 2x magnification
carbon_camera_set_rotation(camera, 45.0f); // Rotate 45 degrees
```

## Coordinate Conversion

```c
// Screen to world (for mouse picking)
float world_x, world_y;
carbon_camera_screen_to_world(camera, mouse_x, mouse_y, &world_x, &world_y);

// Get visible world bounds
float left, right, top, bottom;
carbon_camera_get_bounds(camera, &left, &right, &top, &bottom);
```

## Screen-Space Rendering

```c
// For UI elements that shouldn't move with camera
carbon_sprite_set_camera(sr, NULL);
carbon_sprite_begin(sr, NULL);
// ... draw UI ...
```

## Notes

- Call `carbon_camera_update(camera)` each frame to recompute matrices
- When no camera is set, sprites render in screen-space coordinates

---

# 3D Camera System

Orbital 3D camera with spherical coordinate control and smooth animations.

## Quick Start

```c
#include "carbon/camera3d.h"

Carbon_Camera3D *cam = carbon_camera3d_create();
carbon_camera3d_set_perspective(cam, 60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
carbon_camera3d_set_target(cam, 0, 0, 0);
carbon_camera3d_set_spherical(cam, 45.0f, 30.0f, 15.0f);  // yaw, pitch, distance

// In game loop
carbon_camera3d_update(cam, delta_time);
```

## Orbital Controls

```c
carbon_camera3d_orbit(cam, delta_yaw, delta_pitch);  // Rotate around target
carbon_camera3d_zoom(cam, delta_distance);           // Zoom in/out
carbon_camera3d_pan(cam, right, up);                 // Pan in camera space
carbon_camera3d_pan_xz(cam, dx, dz);                 // Pan in world XZ plane
```

## Constraints

```c
carbon_camera3d_set_distance_limits(cam, 5.0f, 100.0f);
carbon_camera3d_set_pitch_limits(cam, -80.0f, 80.0f);
```

## Smooth Animations

```c
carbon_camera3d_animate_spherical_to(cam, 90.0f, 45.0f, 20.0f, 1.5f);
carbon_camera3d_animate_target_to(cam, 10.0f, 0.0f, 10.0f, 1.0f);
carbon_camera3d_stop_animation(cam);
```

## 3D Picking

```c
float ray_ox, ray_oy, ray_oz, ray_dx, ray_dy, ray_dz;
carbon_camera3d_screen_to_ray(cam, mouse_x, mouse_y, screen_w, screen_h,
                               &ray_ox, &ray_oy, &ray_oz,
                               &ray_dx, &ray_dy, &ray_dz);
```

## Get Matrices

```c
const float *view = carbon_camera3d_get_view_matrix(cam);
const float *proj = carbon_camera3d_get_projection_matrix(cam);
const float *vp = carbon_camera3d_get_vp_matrix(cam);
```
