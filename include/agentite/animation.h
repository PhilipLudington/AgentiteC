/*
 * Carbon Animation System
 *
 * Sprite-based animation with support for sprite sheets, multiple playback
 * modes, and variable frame timing.
 *
 * Usage:
 *   // Create animation from sprite sheet (8 frames in a row, 64x64 each)
 *   Agentite_Animation *walk = agentite_animation_from_grid(texture, 0, 0, 64, 64, 8, 1);
 *   agentite_animation_set_fps(walk, 12.0f);
 *
 *   // Create player to track playback state
 *   Agentite_AnimationPlayer player;
 *   agentite_animation_player_init(&player, walk);
 *   agentite_animation_player_play(&player);
 *
 *   // Each frame:
 *   agentite_animation_player_update(&player, delta_time);
 *   Agentite_Sprite *frame = agentite_animation_player_get_frame(&player);
 *   agentite_sprite_draw(sr, frame, x, y);
 *
 *   // Cleanup
 *   agentite_animation_destroy(walk);
 */

#ifndef AGENTITE_ANIMATION_H
#define AGENTITE_ANIMATION_H

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
typedef enum Agentite_AnimationMode {
    AGENTITE_ANIM_LOOP,       /* Loop from start when finished */
    AGENTITE_ANIM_ONCE,       /* Play once and stop on last frame */
    AGENTITE_ANIM_PING_PONG,  /* Reverse direction at ends */
    AGENTITE_ANIM_ONCE_RESET  /* Play once and reset to first frame */
} Agentite_AnimationMode;

/* Animation definition - frames and timing */
typedef struct Agentite_Animation Agentite_Animation;

/* Animation completion callback */
typedef void (*Agentite_AnimationCallback)(void *userdata);

/* Animation player - tracks playback state for one animation instance */
typedef struct Agentite_AnimationPlayer {
    Agentite_Animation *animation;    /* Current animation */
    uint32_t current_frame;         /* Current frame index */
    float elapsed;                  /* Time elapsed in current frame */
    float speed;                    /* Playback speed multiplier (1.0 = normal) */
    Agentite_AnimationMode mode;      /* Playback mode */
    bool playing;                   /* Is animation playing? */
    bool finished;                  /* Has one-shot animation finished? */
    int direction;                  /* 1 = forward, -1 = reverse (for ping-pong) */

    /* Completion callback (optional) */
    Agentite_AnimationCallback on_complete;
    void *callback_userdata;
} Agentite_AnimationPlayer;

/* ============================================================================
 * Animation Creation
 * ============================================================================ */

/**
 * Create animation from array of sprites (copies the array).
 * Caller OWNS the returned pointer and MUST call agentite_animation_destroy().
 */
Agentite_Animation *agentite_animation_create(Agentite_Sprite *frames, uint32_t frame_count);

/**
 * Create animation from sprite sheet grid.
 * - start_x, start_y: top-left of first frame in pixels
 * - frame_w, frame_h: size of each frame
 * - cols, rows: grid dimensions (total frames = cols * rows)
 * Caller OWNS the returned pointer and MUST call agentite_animation_destroy().
 */
Agentite_Animation *agentite_animation_from_grid(Agentite_Texture *texture,
                                              float start_x, float start_y,
                                              float frame_w, float frame_h,
                                              int cols, int rows);

/**
 * Create animation from horizontal strip (single row).
 * Caller OWNS the returned pointer and MUST call agentite_animation_destroy().
 */
Agentite_Animation *agentite_animation_from_strip(Agentite_Texture *texture,
                                               float start_x, float start_y,
                                               float frame_w, float frame_h,
                                               int frame_count);

/* Destroy animation and free resources */
void agentite_animation_destroy(Agentite_Animation *anim);

/* ============================================================================
 * Animation Properties
 * ============================================================================ */

/* Set frames per second (default: 10) */
void agentite_animation_set_fps(Agentite_Animation *anim, float fps);

/* Set duration for specific frame (overrides fps for that frame) */
void agentite_animation_set_frame_duration(Agentite_Animation *anim, uint32_t frame, float seconds);

/* Get frame count */
uint32_t agentite_animation_get_frame_count(Agentite_Animation *anim);

/* Get specific frame sprite */
Agentite_Sprite *agentite_animation_get_frame(Agentite_Animation *anim, uint32_t index);

/* Get total animation duration in seconds */
float agentite_animation_get_duration(Agentite_Animation *anim);

/* Set origin for all frames */
void agentite_animation_set_origin(Agentite_Animation *anim, float ox, float oy);

/* ============================================================================
 * Animation Player
 * ============================================================================ */

/* Initialize player with animation */
void agentite_animation_player_init(Agentite_AnimationPlayer *player, Agentite_Animation *anim);

/* Update player (call each frame with delta time) */
void agentite_animation_player_update(Agentite_AnimationPlayer *player, float dt);

/* Get current frame sprite */
Agentite_Sprite *agentite_animation_player_get_frame(Agentite_AnimationPlayer *player);

/* Playback control */
void agentite_animation_player_play(Agentite_AnimationPlayer *player);
void agentite_animation_player_pause(Agentite_AnimationPlayer *player);
void agentite_animation_player_stop(Agentite_AnimationPlayer *player);   /* Stop and reset */
void agentite_animation_player_restart(Agentite_AnimationPlayer *player); /* Restart from beginning */

/* Set playback mode */
void agentite_animation_player_set_mode(Agentite_AnimationPlayer *player, Agentite_AnimationMode mode);

/* Set playback speed (1.0 = normal, 2.0 = double speed, etc.) */
void agentite_animation_player_set_speed(Agentite_AnimationPlayer *player, float speed);

/* Jump to specific frame */
void agentite_animation_player_set_frame(Agentite_AnimationPlayer *player, uint32_t frame);

/* Set completion callback (called when ONCE/ONCE_RESET finishes, or each loop) */
void agentite_animation_player_set_callback(Agentite_AnimationPlayer *player,
                                          Agentite_AnimationCallback callback,
                                          void *userdata);

/* Switch to different animation (resets playback state) */
void agentite_animation_player_set_animation(Agentite_AnimationPlayer *player,
                                            Agentite_Animation *anim);

/* Query state */
bool agentite_animation_player_is_playing(Agentite_AnimationPlayer *player);
bool agentite_animation_player_is_finished(Agentite_AnimationPlayer *player);
uint32_t agentite_animation_player_get_current_frame(Agentite_AnimationPlayer *player);
float agentite_animation_player_get_progress(Agentite_AnimationPlayer *player); /* 0.0 to 1.0 */

/* ============================================================================
 * Convenience: Draw animated sprite directly
 * ============================================================================ */

/* Draw current animation frame at position */
void agentite_animation_draw(Agentite_SpriteRenderer *sr, Agentite_AnimationPlayer *player,
                           float x, float y);

/* Draw with scale */
void agentite_animation_draw_scaled(Agentite_SpriteRenderer *sr, Agentite_AnimationPlayer *player,
                                  float x, float y, float scale_x, float scale_y);

/* Draw with full transform */
void agentite_animation_draw_ex(Agentite_SpriteRenderer *sr, Agentite_AnimationPlayer *player,
                              float x, float y,
                              float scale_x, float scale_y,
                              float rotation_deg,
                              float origin_x, float origin_y);

/* Draw with tint */
void agentite_animation_draw_tinted(Agentite_SpriteRenderer *sr, Agentite_AnimationPlayer *player,
                                  float x, float y,
                                  float r, float g, float b, float a);

/* Draw with full options */
void agentite_animation_draw_full(Agentite_SpriteRenderer *sr, Agentite_AnimationPlayer *player,
                                float x, float y,
                                float scale_x, float scale_y,
                                float rotation_deg,
                                float origin_x, float origin_y,
                                float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_ANIMATION_H */
