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
Agentite_Texture *sheet = agentite_texture_load(sr, "spritesheet.png");

// Create from horizontal strip (x, y, frame_width, frame_height, frame_count)
Agentite_Animation *walk = agentite_animation_from_strip(sheet, 0, 0, 64, 64, 8);

// Create from grid (x, y, frame_width, frame_height, cols, rows)
Agentite_Animation *idle = agentite_animation_from_grid(sheet, 0, 64, 64, 64, 4, 2);

// Set timing
agentite_animation_set_fps(walk, 12.0f);
agentite_animation_set_frame_duration(walk, 7, 0.2f);  // Custom duration for frame 7
```

### Animation Players
```c
Agentite_AnimationPlayer player;
agentite_animation_player_init(&player, animation);

// Playback modes
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_LOOP);       // Default
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_ONCE);       // Stop on last frame
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_PING_PONG);  // Reverse at ends
agentite_animation_player_set_mode(&player, AGENTITE_ANIM_ONCE_RESET); // Reset after playing

// Control
agentite_animation_player_play(&player);
agentite_animation_player_pause(&player);
agentite_animation_player_stop(&player);    // Stop and reset to frame 0
agentite_animation_player_restart(&player); // Restart from beginning

// Speed
agentite_animation_player_set_speed(&player, 2.0f);  // Double speed
```

### Rendering
```c
// In game loop
agentite_animation_player_update(&player, delta_time);

// During sprite batch
agentite_sprite_begin(sprites, NULL);
agentite_animation_draw(sprites, &player, x, y);
agentite_animation_draw_scaled(sprites, &player, x, y, sx, sy);
agentite_animation_draw_ex(sprites, &player, x, y, sx, sy, rotation, ox, oy);
agentite_sprite_upload(sprites, cmd);
// ... render pass ...
agentite_sprite_render(sprites, cmd, pass);
```

### Completion Callbacks
```c
void on_complete(void *userdata) {
    // Animation finished (useful for state machines)
}
agentite_animation_player_set_callback(&player, on_complete, userdata);
```
