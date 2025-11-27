/*
 * Carbon Animation System
 *
 * Sprite-based animation with support for sprite sheets, multiple playback
 * modes, and variable frame timing.
 *
 * Usage:
 *   // Create animation from sprite sheet (8 frames in a row, 64x64 each)
 *   Carbon_Animation *walk = carbon_animation_from_grid(texture, 0, 0, 64, 64, 8, 1);
 *   carbon_animation_set_fps(walk, 12.0f);
 *
 *   // Create player to track playback state
 *   Carbon_AnimationPlayer player;
 *   carbon_animation_player_init(&player, walk);
 *   carbon_animation_player_play(&player);
 *
 *   // Each frame:
 *   carbon_animation_player_update(&player, delta_time);
 *   Carbon_Sprite *frame = carbon_animation_player_get_frame(&player);
 *   carbon_sprite_draw(sr, frame, x, y);
 *
 *   // Cleanup
 *   carbon_animation_destroy(walk);
 */

#ifndef CARBON_ANIMATION_H
#define CARBON_ANIMATION_H

#include "sprite.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Playback mode */
typedef enum Carbon_AnimationMode {
    CARBON_ANIM_LOOP,       /* Loop from start when finished */
    CARBON_ANIM_ONCE,       /* Play once and stop on last frame */
    CARBON_ANIM_PING_PONG,  /* Reverse direction at ends */
    CARBON_ANIM_ONCE_RESET  /* Play once and reset to first frame */
} Carbon_AnimationMode;

/* Animation definition - frames and timing */
typedef struct Carbon_Animation Carbon_Animation;

/* Animation completion callback */
typedef void (*Carbon_AnimationCallback)(void *userdata);

/* Animation player - tracks playback state for one animation instance */
typedef struct Carbon_AnimationPlayer {
    Carbon_Animation *animation;    /* Current animation */
    uint32_t current_frame;         /* Current frame index */
    float elapsed;                  /* Time elapsed in current frame */
    float speed;                    /* Playback speed multiplier (1.0 = normal) */
    Carbon_AnimationMode mode;      /* Playback mode */
    bool playing;                   /* Is animation playing? */
    bool finished;                  /* Has one-shot animation finished? */
    int direction;                  /* 1 = forward, -1 = reverse (for ping-pong) */

    /* Completion callback (optional) */
    Carbon_AnimationCallback on_complete;
    void *callback_userdata;
} Carbon_AnimationPlayer;

/* ============================================================================
 * Animation Creation
 * ============================================================================ */

/* Create animation from array of sprites (copies the array) */
Carbon_Animation *carbon_animation_create(Carbon_Sprite *frames, uint32_t frame_count);

/* Create animation from sprite sheet grid
 * - start_x, start_y: top-left of first frame in pixels
 * - frame_w, frame_h: size of each frame
 * - cols, rows: grid dimensions (total frames = cols * rows)
 */
Carbon_Animation *carbon_animation_from_grid(Carbon_Texture *texture,
                                              float start_x, float start_y,
                                              float frame_w, float frame_h,
                                              int cols, int rows);

/* Create animation from horizontal strip (single row) */
Carbon_Animation *carbon_animation_from_strip(Carbon_Texture *texture,
                                               float start_x, float start_y,
                                               float frame_w, float frame_h,
                                               int frame_count);

/* Destroy animation */
void carbon_animation_destroy(Carbon_Animation *anim);

/* ============================================================================
 * Animation Properties
 * ============================================================================ */

/* Set frames per second (default: 10) */
void carbon_animation_set_fps(Carbon_Animation *anim, float fps);

/* Set duration for specific frame (overrides fps for that frame) */
void carbon_animation_set_frame_duration(Carbon_Animation *anim, uint32_t frame, float seconds);

/* Get frame count */
uint32_t carbon_animation_get_frame_count(Carbon_Animation *anim);

/* Get specific frame sprite */
Carbon_Sprite *carbon_animation_get_frame(Carbon_Animation *anim, uint32_t index);

/* Get total animation duration in seconds */
float carbon_animation_get_duration(Carbon_Animation *anim);

/* Set origin for all frames */
void carbon_animation_set_origin(Carbon_Animation *anim, float ox, float oy);

/* ============================================================================
 * Animation Player
 * ============================================================================ */

/* Initialize player with animation */
void carbon_animation_player_init(Carbon_AnimationPlayer *player, Carbon_Animation *anim);

/* Update player (call each frame with delta time) */
void carbon_animation_player_update(Carbon_AnimationPlayer *player, float dt);

/* Get current frame sprite */
Carbon_Sprite *carbon_animation_player_get_frame(Carbon_AnimationPlayer *player);

/* Playback control */
void carbon_animation_player_play(Carbon_AnimationPlayer *player);
void carbon_animation_player_pause(Carbon_AnimationPlayer *player);
void carbon_animation_player_stop(Carbon_AnimationPlayer *player);   /* Stop and reset */
void carbon_animation_player_restart(Carbon_AnimationPlayer *player); /* Restart from beginning */

/* Set playback mode */
void carbon_animation_player_set_mode(Carbon_AnimationPlayer *player, Carbon_AnimationMode mode);

/* Set playback speed (1.0 = normal, 2.0 = double speed, etc.) */
void carbon_animation_player_set_speed(Carbon_AnimationPlayer *player, float speed);

/* Jump to specific frame */
void carbon_animation_player_set_frame(Carbon_AnimationPlayer *player, uint32_t frame);

/* Set completion callback (called when ONCE/ONCE_RESET finishes, or each loop) */
void carbon_animation_player_set_callback(Carbon_AnimationPlayer *player,
                                          Carbon_AnimationCallback callback,
                                          void *userdata);

/* Switch to different animation (resets playback state) */
void carbon_animation_player_set_animation(Carbon_AnimationPlayer *player,
                                            Carbon_Animation *anim);

/* Query state */
bool carbon_animation_player_is_playing(Carbon_AnimationPlayer *player);
bool carbon_animation_player_is_finished(Carbon_AnimationPlayer *player);
uint32_t carbon_animation_player_get_current_frame(Carbon_AnimationPlayer *player);
float carbon_animation_player_get_progress(Carbon_AnimationPlayer *player); /* 0.0 to 1.0 */

/* ============================================================================
 * Convenience: Draw animated sprite directly
 * ============================================================================ */

/* Draw current animation frame at position */
void carbon_animation_draw(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                           float x, float y);

/* Draw with scale */
void carbon_animation_draw_scaled(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                                  float x, float y, float scale_x, float scale_y);

/* Draw with full transform */
void carbon_animation_draw_ex(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                              float x, float y,
                              float scale_x, float scale_y,
                              float rotation_deg,
                              float origin_x, float origin_y);

/* Draw with tint */
void carbon_animation_draw_tinted(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                                  float x, float y,
                                  float r, float g, float b, float a);

/* Draw with full options */
void carbon_animation_draw_full(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                                float x, float y,
                                float scale_x, float scale_y,
                                float rotation_deg,
                                float origin_x, float origin_y,
                                float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_ANIMATION_H */
