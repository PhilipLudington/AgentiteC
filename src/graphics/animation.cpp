/*
 * Carbon Animation System Implementation
 */

#include "carbon/carbon.h"
#include "carbon/animation.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct Carbon_Animation {
    Carbon_Sprite *frames;      /* Array of frame sprites */
    float *durations;           /* Per-frame duration in seconds (NULL = use default_duration) */
    uint32_t frame_count;       /* Number of frames */
    float default_duration;     /* Default frame duration (1/fps) */
};

/* ============================================================================
 * Animation Creation
 * ============================================================================ */

Carbon_Animation *carbon_animation_create(Carbon_Sprite *frames, uint32_t frame_count)
{
    if (!frames || frame_count == 0) {
        return NULL;
    }

    Carbon_Animation *anim = CARBON_ALLOC(Carbon_Animation);
    if (!anim) {
        return NULL;
    }

    anim->frames = (Carbon_Sprite*)malloc(frame_count * sizeof(Carbon_Sprite));
    if (!anim->frames) {
        free(anim);
        return NULL;
    }

    memcpy(anim->frames, frames, frame_count * sizeof(Carbon_Sprite));
    anim->frame_count = frame_count;
    anim->default_duration = 0.1f;  /* 10 fps default */
    anim->durations = NULL;         /* Use default for all frames */

    return anim;
}

Carbon_Animation *carbon_animation_from_grid(Carbon_Texture *texture,
                                              float start_x, float start_y,
                                              float frame_w, float frame_h,
                                              int cols, int rows)
{
    if (!texture || cols <= 0 || rows <= 0) {
        return NULL;
    }

    uint32_t frame_count = (uint32_t)(cols * rows);
    Carbon_Sprite *frames = (Carbon_Sprite*)malloc(frame_count * sizeof(Carbon_Sprite));
    if (!frames) {
        return NULL;
    }

    /* Generate frames from grid (left-to-right, top-to-bottom) */
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            uint32_t idx = (uint32_t)(y * cols + x);
            frames[idx] = carbon_sprite_create(
                texture,
                start_x + x * frame_w,
                start_y + y * frame_h,
                frame_w,
                frame_h
            );
        }
    }

    Carbon_Animation *anim = carbon_animation_create(frames, frame_count);
    free(frames);

    return anim;
}

Carbon_Animation *carbon_animation_from_strip(Carbon_Texture *texture,
                                               float start_x, float start_y,
                                               float frame_w, float frame_h,
                                               int frame_count)
{
    return carbon_animation_from_grid(texture, start_x, start_y,
                                      frame_w, frame_h, frame_count, 1);
}

void carbon_animation_destroy(Carbon_Animation *anim)
{
    if (!anim) {
        return;
    }

    free(anim->frames);
    free(anim->durations);
    free(anim);
}

/* ============================================================================
 * Animation Properties
 * ============================================================================ */

void carbon_animation_set_fps(Carbon_Animation *anim, float fps)
{
    if (!anim || fps <= 0.0f) {
        return;
    }
    anim->default_duration = 1.0f / fps;
}

void carbon_animation_set_frame_duration(Carbon_Animation *anim, uint32_t frame, float seconds)
{
    if (!anim || frame >= anim->frame_count || seconds <= 0.0f) {
        return;
    }

    /* Allocate per-frame durations array if needed */
    if (!anim->durations) {
        anim->durations = (float*)malloc(anim->frame_count * sizeof(float));
        if (!anim->durations) {
            return;
        }
        /* Initialize all to default */
        for (uint32_t i = 0; i < anim->frame_count; i++) {
            anim->durations[i] = anim->default_duration;
        }
    }

    anim->durations[frame] = seconds;
}

uint32_t carbon_animation_get_frame_count(Carbon_Animation *anim)
{
    return anim ? anim->frame_count : 0;
}

Carbon_Sprite *carbon_animation_get_frame(Carbon_Animation *anim, uint32_t index)
{
    if (!anim || index >= anim->frame_count) {
        return NULL;
    }
    return &anim->frames[index];
}

float carbon_animation_get_duration(Carbon_Animation *anim)
{
    if (!anim) {
        return 0.0f;
    }

    float total = 0.0f;
    if (anim->durations) {
        for (uint32_t i = 0; i < anim->frame_count; i++) {
            total += anim->durations[i];
        }
    } else {
        total = anim->default_duration * anim->frame_count;
    }
    return total;
}

void carbon_animation_set_origin(Carbon_Animation *anim, float ox, float oy)
{
    if (!anim) {
        return;
    }

    for (uint32_t i = 0; i < anim->frame_count; i++) {
        carbon_sprite_set_origin(&anim->frames[i], ox, oy);
    }
}

/* ============================================================================
 * Animation Player
 * ============================================================================ */

void carbon_animation_player_init(Carbon_AnimationPlayer *player, Carbon_Animation *anim)
{
    if (!player) {
        return;
    }

    player->animation = anim;
    player->current_frame = 0;
    player->elapsed = 0.0f;
    player->speed = 1.0f;
    player->mode = CARBON_ANIM_LOOP;
    player->playing = false;
    player->finished = false;
    player->direction = 1;
    player->on_complete = NULL;
    player->callback_userdata = NULL;
}

static float get_frame_duration(Carbon_Animation *anim, uint32_t frame)
{
    if (!anim) {
        return 0.1f;
    }
    if (anim->durations && frame < anim->frame_count) {
        return anim->durations[frame];
    }
    return anim->default_duration;
}

void carbon_animation_player_update(Carbon_AnimationPlayer *player, float dt)
{
    if (!player || !player->animation || !player->playing || player->finished) {
        return;
    }

    Carbon_Animation *anim = player->animation;
    float frame_dur = get_frame_duration(anim, player->current_frame);

    player->elapsed += dt * player->speed;

    /* Advance frames while accumulated time exceeds frame duration */
    while (player->elapsed >= frame_dur && !player->finished) {
        player->elapsed -= frame_dur;

        /* Calculate next frame based on direction */
        int next_frame = (int)player->current_frame + player->direction;

        switch (player->mode) {
            case CARBON_ANIM_LOOP:
                if (next_frame >= (int)anim->frame_count) {
                    next_frame = 0;
                    if (player->on_complete) {
                        player->on_complete(player->callback_userdata);
                    }
                } else if (next_frame < 0) {
                    next_frame = (int)anim->frame_count - 1;
                }
                break;

            case CARBON_ANIM_ONCE:
                if (next_frame >= (int)anim->frame_count) {
                    next_frame = (int)anim->frame_count - 1;
                    player->finished = true;
                    player->playing = false;
                    if (player->on_complete) {
                        player->on_complete(player->callback_userdata);
                    }
                } else if (next_frame < 0) {
                    next_frame = 0;
                    player->finished = true;
                    player->playing = false;
                }
                break;

            case CARBON_ANIM_ONCE_RESET:
                if (next_frame >= (int)anim->frame_count || next_frame < 0) {
                    next_frame = 0;
                    player->finished = true;
                    player->playing = false;
                    if (player->on_complete) {
                        player->on_complete(player->callback_userdata);
                    }
                }
                break;

            case CARBON_ANIM_PING_PONG:
                if (next_frame >= (int)anim->frame_count) {
                    player->direction = -1;
                    next_frame = (int)anim->frame_count - 2;
                    if (next_frame < 0) next_frame = 0;
                    if (player->on_complete) {
                        player->on_complete(player->callback_userdata);
                    }
                } else if (next_frame < 0) {
                    player->direction = 1;
                    next_frame = 1;
                    if (next_frame >= (int)anim->frame_count) {
                        next_frame = 0;
                    }
                    if (player->on_complete) {
                        player->on_complete(player->callback_userdata);
                    }
                }
                break;
        }

        player->current_frame = (uint32_t)next_frame;
        frame_dur = get_frame_duration(anim, player->current_frame);
    }
}

Carbon_Sprite *carbon_animation_player_get_frame(Carbon_AnimationPlayer *player)
{
    if (!player || !player->animation) {
        return NULL;
    }
    return carbon_animation_get_frame(player->animation, player->current_frame);
}

void carbon_animation_player_play(Carbon_AnimationPlayer *player)
{
    if (!player) {
        return;
    }
    player->playing = true;
    player->finished = false;
}

void carbon_animation_player_pause(Carbon_AnimationPlayer *player)
{
    if (!player) {
        return;
    }
    player->playing = false;
}

void carbon_animation_player_stop(Carbon_AnimationPlayer *player)
{
    if (!player) {
        return;
    }
    player->playing = false;
    player->current_frame = 0;
    player->elapsed = 0.0f;
    player->finished = false;
    player->direction = 1;
}

void carbon_animation_player_restart(Carbon_AnimationPlayer *player)
{
    carbon_animation_player_stop(player);
    carbon_animation_player_play(player);
}

void carbon_animation_player_set_mode(Carbon_AnimationPlayer *player, Carbon_AnimationMode mode)
{
    if (!player) {
        return;
    }
    player->mode = mode;
}

void carbon_animation_player_set_speed(Carbon_AnimationPlayer *player, float speed)
{
    if (!player || speed < 0.0f) {
        return;
    }
    player->speed = speed;
}

void carbon_animation_player_set_frame(Carbon_AnimationPlayer *player, uint32_t frame)
{
    if (!player || !player->animation) {
        return;
    }
    if (frame >= player->animation->frame_count) {
        frame = player->animation->frame_count - 1;
    }
    player->current_frame = frame;
    player->elapsed = 0.0f;
}

void carbon_animation_player_set_callback(Carbon_AnimationPlayer *player,
                                          Carbon_AnimationCallback callback,
                                          void *userdata)
{
    if (!player) {
        return;
    }
    player->on_complete = callback;
    player->callback_userdata = userdata;
}

void carbon_animation_player_set_animation(Carbon_AnimationPlayer *player,
                                            Carbon_Animation *anim)
{
    if (!player) {
        return;
    }

    bool was_playing = player->playing;
    carbon_animation_player_stop(player);
    player->animation = anim;

    if (was_playing) {
        carbon_animation_player_play(player);
    }
}

bool carbon_animation_player_is_playing(Carbon_AnimationPlayer *player)
{
    return player ? player->playing : false;
}

bool carbon_animation_player_is_finished(Carbon_AnimationPlayer *player)
{
    return player ? player->finished : false;
}

uint32_t carbon_animation_player_get_current_frame(Carbon_AnimationPlayer *player)
{
    return player ? player->current_frame : 0;
}

float carbon_animation_player_get_progress(Carbon_AnimationPlayer *player)
{
    if (!player || !player->animation || player->animation->frame_count == 0) {
        return 0.0f;
    }

    float total_duration = carbon_animation_get_duration(player->animation);
    if (total_duration <= 0.0f) {
        return 0.0f;
    }

    /* Calculate time up to current frame */
    float elapsed_total = 0.0f;
    for (uint32_t i = 0; i < player->current_frame; i++) {
        elapsed_total += get_frame_duration(player->animation, i);
    }
    elapsed_total += player->elapsed;

    return elapsed_total / total_duration;
}

/* ============================================================================
 * Convenience Drawing Functions
 * ============================================================================ */

void carbon_animation_draw(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                           float x, float y)
{
    Carbon_Sprite *frame = carbon_animation_player_get_frame(player);
    if (frame) {
        carbon_sprite_draw(sr, frame, x, y);
    }
}

void carbon_animation_draw_scaled(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                                  float x, float y, float scale_x, float scale_y)
{
    Carbon_Sprite *frame = carbon_animation_player_get_frame(player);
    if (frame) {
        carbon_sprite_draw_scaled(sr, frame, x, y, scale_x, scale_y);
    }
}

void carbon_animation_draw_ex(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                              float x, float y,
                              float scale_x, float scale_y,
                              float rotation_deg,
                              float origin_x, float origin_y)
{
    Carbon_Sprite *frame = carbon_animation_player_get_frame(player);
    if (frame) {
        carbon_sprite_draw_ex(sr, frame, x, y, scale_x, scale_y,
                              rotation_deg, origin_x, origin_y);
    }
}

void carbon_animation_draw_tinted(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                                  float x, float y,
                                  float r, float g, float b, float a)
{
    Carbon_Sprite *frame = carbon_animation_player_get_frame(player);
    if (frame) {
        carbon_sprite_draw_tinted(sr, frame, x, y, r, g, b, a);
    }
}

void carbon_animation_draw_full(Carbon_SpriteRenderer *sr, Carbon_AnimationPlayer *player,
                                float x, float y,
                                float scale_x, float scale_y,
                                float rotation_deg,
                                float origin_x, float origin_y,
                                float r, float g, float b, float a)
{
    Carbon_Sprite *frame = carbon_animation_player_get_frame(player);
    if (frame) {
        carbon_sprite_draw_full(sr, frame, x, y, scale_x, scale_y,
                                rotation_deg, origin_x, origin_y, r, g, b, a);
    }
}
