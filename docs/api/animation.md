# Animation System

Sprite-based animation with support for sprite sheets, multiple playback modes, and variable frame timing.

## Quick Start

```c
#include "carbon/animation.h"

// Load sprite sheet texture
Carbon_Texture *sheet = carbon_texture_load(sr, "assets/player_walk.png");

// Create animation from sprite sheet (8 frames, 64x64 each)
Carbon_Animation *walk = carbon_animation_from_strip(sheet, 0, 0, 64, 64, 8);
carbon_animation_set_fps(walk, 12.0f);

// Create player to track playback state
Carbon_AnimationPlayer player;
carbon_animation_player_init(&player, walk);
carbon_animation_player_play(&player);

// In game loop:
carbon_animation_player_update(&player, delta_time);

// Draw (during sprite batch)
carbon_sprite_begin(sr, NULL);
carbon_animation_draw(sr, &player, x, y);
carbon_sprite_upload(sr, cmd);
```

## Creating Animations

```c
// From horizontal strip (frames in a row)
Carbon_Animation *anim = carbon_animation_from_strip(tex, start_x, start_y,
                                                      frame_w, frame_h, frame_count);

// From grid (multiple rows)
Carbon_Animation *anim = carbon_animation_from_grid(tex, start_x, start_y,
                                                     frame_w, frame_h, cols, rows);
```

## Playback Modes

```c
carbon_animation_player_set_mode(&player, CARBON_ANIM_LOOP);       // Loop forever (default)
carbon_animation_player_set_mode(&player, CARBON_ANIM_ONCE);       // Play once, stop on last
carbon_animation_player_set_mode(&player, CARBON_ANIM_PING_PONG);  // Reverse at ends
carbon_animation_player_set_mode(&player, CARBON_ANIM_ONCE_RESET); // Play once, reset to first
```

## Playback Control

```c
carbon_animation_player_play(&player);
carbon_animation_player_pause(&player);
carbon_animation_player_stop(&player);       // Stop and reset to frame 0
carbon_animation_player_restart(&player);    // Restart from beginning
carbon_animation_player_set_speed(&player, 2.0f);   // Double speed
carbon_animation_player_set_frame(&player, 3);      // Jump to frame 3
carbon_animation_player_set_animation(&player, idle); // Switch animation
```

## Query State

```c
bool playing = carbon_animation_player_is_playing(&player);
bool finished = carbon_animation_player_is_finished(&player);  // For ONCE mode
uint32_t frame = carbon_animation_player_get_current_frame(&player);
float progress = carbon_animation_player_get_progress(&player);  // 0.0 to 1.0
```

## Completion Callback

```c
void on_attack_done(void *userdata) {
    // Switch back to idle animation
}
carbon_animation_player_set_callback(&player, on_attack_done, NULL);
```

## Variable Frame Timing

```c
// Hold specific frame longer
carbon_animation_set_frame_duration(walk, 7, 0.2f);  // Frame 7 = 0.2 seconds
```

## Draw Variants

```c
carbon_animation_draw(sr, &player, x, y);
carbon_animation_draw_scaled(sr, &player, x, y, 2.0f, 2.0f);
carbon_animation_draw_ex(sr, &player, x, y, sx, sy, rotation, ox, oy);
carbon_animation_draw_tinted(sr, &player, x, y, r, g, b, a);
```

## Notes

- All frames share the same texture, so they batch efficiently
- Draws through the sprite renderer (same batching rules apply)
- Set origin with `carbon_animation_set_origin(anim, 0.5f, 1.0f)` for rotation pivot
