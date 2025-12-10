# Animation System

Sprite-based animation with support for sprite sheets, multiple playback modes, and variable frame timing.

## Quick Start

```c
#include "agentite/animation.h"

// Load sprite sheet texture
Agentite_Texture *sheet = agentite_texture_load(sr, "assets/player_walk.png");

// Create animation from sprite sheet (8 frames, 64x64 each)
Agentite_Animation *walk = agentite_animation_from_strip(sheet, 0, 0, 64, 64, 8);
agentite_animation_set_fps(walk, 12.0f);

// Create player to track playback state
Agentite_AnimationPlayer player;
agentite_animation_player_init(&player, walk);
agentite_animation_player_play(&player);

// In game loop:
agentite_animation_player_update(&player, delta_time);

// Draw (during sprite batch)
agentite_sprite_begin(sr, NULL);
agentite_animation_draw(sr, &player, x, y);
agentite_sprite_upload(sr, cmd);
```

## Creating Animations

```c
// From horizontal strip (frames in a row)
Agentite_Animation *anim = agentite_animation_from_strip(tex, start_x, start_y,
                                                      frame_w, frame_h, frame_count);

// From grid (multiple rows)
Agentite_Animation *anim = agentite_animation_from_grid(tex, start_x, start_y,
                                                     frame_w, frame_h, cols, rows);
```

## Playback Modes

```c
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_LOOP);       // Loop forever (default)
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_ONCE);       // Play once, stop on last
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_PING_PONG);  // Reverse at ends
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_ONCE_RESET); // Play once, reset to first
```

## Playback Control

```c
agentite_animation_player_play(&player);
agentite_animation_player_pause(&player);
agentite_animation_player_stop(&player);       // Stop and reset to frame 0
agentite_animation_player_restart(&player);    // Restart from beginning
agentite_animation_player_set_speed(&player, 2.0f);   // Double speed
agentite_animation_player_set_frame(&player, 3);      // Jump to frame 3
agentite_animation_player_set_animation(&player, idle); // Switch animation
```

## Query State

```c
bool playing = agentite_animation_player_is_playing(&player);
bool finished = agentite_animation_player_is_finished(&player);  // For ONCE mode
uint32_t frame = agentite_animation_player_get_current_frame(&player);
float progress = agentite_animation_player_get_progress(&player);  // 0.0 to 1.0
```

## Completion Callback

```c
void on_attack_done(void *userdata) {
    // Switch back to idle animation
}
agentite_animation_player_set_callback(&player, on_attack_done, NULL);
```

## Variable Frame Timing

```c
// Hold specific frame longer
agentite_animation_set_frame_duration(walk, 7, 0.2f);  // Frame 7 = 0.2 seconds
```

## Draw Variants

```c
agentite_animation_draw(sr, &player, x, y);
agentite_animation_draw_scaled(sr, &player, x, y, 2.0f, 2.0f);
agentite_animation_draw_ex(sr, &player, x, y, sx, sy, rotation, ox, oy);
agentite_animation_draw_tinted(sr, &player, x, y, r, g, b, a);
```

## Notes

- All frames share the same texture, so they batch efficiently
- Draws through the sprite renderer (same batching rules apply)
- Set origin with `agentite_animation_set_origin(anim, 0.5f, 1.0f)` for rotation pivot
