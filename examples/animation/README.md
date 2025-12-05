# Animation Example

Demonstrates sprite-based animation with the Carbon animation system.

## What This Demonstrates

- Creating animations from sprite sheets
- Animation playback modes: loop, once, ping-pong
- Animation player controls (play, pause, stop, restart)
- Playback speed adjustment
- Completion callbacks

## Running

```bash
make example-animation
```

## Controls

| Key | Action |
|-----|--------|
| Space | Play one-shot animation |
| R | Restart all animations |
| Escape | Quit |

## Key Patterns

### Creating Animations
```c
// Load sprite sheet texture
Carbon_Texture *sheet = carbon_texture_load(sr, "spritesheet.png");

// Create from horizontal strip (x, y, frame_width, frame_height, frame_count)
Carbon_Animation *walk = carbon_animation_from_strip(sheet, 0, 0, 64, 64, 8);

// Create from grid (x, y, frame_width, frame_height, cols, rows)
Carbon_Animation *idle = carbon_animation_from_grid(sheet, 0, 64, 64, 64, 4, 2);

// Set timing
carbon_animation_set_fps(walk, 12.0f);
carbon_animation_set_frame_duration(walk, 7, 0.2f);  // Custom duration for frame 7
```

### Animation Players
```c
Carbon_AnimationPlayer player;
carbon_animation_player_init(&player, animation);

// Playback modes
carbon_animation_player_set_mode(&player, CARBON_ANIM_LOOP);       // Default
carbon_animation_player_set_mode(&player, CARBON_ANIM_ONCE);       // Stop on last frame
carbon_animation_player_set_mode(&player, CARBON_ANIM_PING_PONG);  // Reverse at ends
carbon_animation_player_set_mode(&player, CARBON_ANIM_ONCE_RESET); // Reset after playing

// Control
carbon_animation_player_play(&player);
carbon_animation_player_pause(&player);
carbon_animation_player_stop(&player);    // Stop and reset to frame 0
carbon_animation_player_restart(&player); // Restart from beginning

// Speed
carbon_animation_player_set_speed(&player, 2.0f);  // Double speed
```

### Rendering
```c
// In game loop
carbon_animation_player_update(&player, delta_time);

// During sprite batch
carbon_sprite_begin(sprites, NULL);
carbon_animation_draw(sprites, &player, x, y);
carbon_animation_draw_scaled(sprites, &player, x, y, sx, sy);
carbon_animation_draw_ex(sprites, &player, x, y, sx, sy, rotation, ox, oy);
carbon_sprite_upload(sprites, cmd);
// ... render pass ...
carbon_sprite_render(sprites, cmd, pass);
```

### Completion Callbacks
```c
void on_complete(void *userdata) {
    // Animation finished (useful for state machines)
}
carbon_animation_player_set_callback(&player, on_complete, userdata);
```
