/**
 * Agentite Engine - Particle System Example
 *
 * Demonstrates the particle system with various emitter types, effects,
 * and built-in presets. Use number keys to spawn different effects:
 *
 *   1 - Explosion burst
 *   2 - Fire (continuous)
 *   3 - Smoke (continuous)
 *   4 - Sparks burst
 *   5 - Rain (toggle)
 *   6 - Snow (toggle)
 *   7 - Custom emitter (circle emitter with gravity)
 *   8 - Trail effect (follows mouse)
 *
 *   SPACE - Clear all particles
 *   R - Reset (destroy all emitters)
 *   ESC - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/particle.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

/* Active effect emitters (toggled on/off) */
static Agentite_ParticleEmitter *s_rain_emitter = NULL;
static Agentite_ParticleEmitter *s_snow_emitter = NULL;
static Agentite_ParticleEmitter *s_trail_emitter = NULL;

/* Shared particle texture */
static Agentite_Texture *s_particle_texture = NULL;

/* Create a simple circular gradient texture for particles */
static Agentite_Texture *create_particle_texture(Agentite_SpriteRenderer *sr) {
    const int size = 32;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    float center = size / 2.0f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x - center + 0.5f;
            float dy = y - center + 0.5f;
            float dist = sqrtf(dx * dx + dy * dy) / center;

            /* Smooth circular gradient */
            float alpha = 1.0f - dist;
            if (alpha < 0.0f) alpha = 0.0f;
            alpha = alpha * alpha;  /* Smoother falloff */

            int idx = (y * size + x) * 4;
            pixels[idx + 0] = 255;  /* R */
            pixels[idx + 1] = 255;  /* G */
            pixels[idx + 2] = 255;  /* B */
            pixels[idx + 3] = (unsigned char)(alpha * 255);  /* A */
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* Create a custom particle emitter demonstrating manual configuration */
static Agentite_ParticleEmitter *create_custom_emitter(
    Agentite_ParticleSystem *ps,
    float x, float y)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;

    /* Circle shape emitter */
    cfg.shape = AGENTITE_EMITTER_CIRCLE;
    cfg.radius = 30.0f;

    /* Continuous emission */
    cfg.mode = AGENTITE_EMISSION_CONTINUOUS;
    cfg.emission_rate = 50.0f;

    /* Particle properties */
    cfg.particle.lifetime_min = 2.0f;
    cfg.particle.lifetime_max = 3.0f;

    /* Upward velocity with spread */
    cfg.particle.speed_min = 80.0f;
    cfg.particle.speed_max = 120.0f;
    cfg.particle.direction_min = 60.0f;   /* 60 degrees (mostly up) */
    cfg.particle.direction_max = 120.0f;  /* 120 degrees (mostly up) */

    /* Gravity pulls particles down */
    cfg.particle.gravity = 100.0f;
    cfg.particle.drag = 0.05f;

    /* Size grows then shrinks */
    cfg.particle.start_size_min = 8.0f;
    cfg.particle.start_size_max = 12.0f;
    cfg.particle.end_size_min = 2.0f;
    cfg.particle.end_size_max = 4.0f;
    cfg.particle.size_ease = AGENTITE_EASE_OUT_QUAD;

    /* Color: cyan to purple, fade out */
    cfg.particle.start_color = (Agentite_Color){0.3f, 0.8f, 1.0f, 1.0f};
    cfg.particle.end_color = (Agentite_Color){0.8f, 0.2f, 1.0f, 0.0f};
    cfg.particle.color_ease = AGENTITE_EASE_IN_OUT_QUAD;

    /* Some rotation */
    cfg.particle.start_rotation_min = 0.0f;
    cfg.particle.start_rotation_max = 360.0f;
    cfg.particle.angular_velocity_min = -90.0f;
    cfg.particle.angular_velocity_max = 90.0f;

    /* Additive blending for glow effect */
    cfg.blend = AGENTITE_BLEND_ADDITIVE;

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_texture(emitter, s_particle_texture);
        agentite_particle_emitter_set_position(emitter, x, y);
        agentite_particle_emitter_start(emitter);
    }
    return emitter;
}

/* Create rectangle emitter for demonstrating different shapes */
static Agentite_ParticleEmitter *create_rectangle_emitter(
    Agentite_ParticleSystem *ps,
    float x, float y)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;

    /* Rectangle shape emitter */
    cfg.shape = AGENTITE_EMITTER_RECTANGLE;
    cfg.width = 200.0f;
    cfg.height = 10.0f;

    /* Burst mode */
    cfg.mode = AGENTITE_EMISSION_BURST;
    cfg.burst_count = 50;

    /* Particle properties - downward cascade */
    cfg.particle.lifetime_min = 1.0f;
    cfg.particle.lifetime_max = 2.0f;
    cfg.particle.speed_min = 50.0f;
    cfg.particle.speed_max = 150.0f;
    cfg.particle.direction_min = 250.0f;  /* Mostly downward */
    cfg.particle.direction_max = 290.0f;
    cfg.particle.gravity = 200.0f;

    cfg.particle.start_size_min = 4.0f;
    cfg.particle.start_size_max = 8.0f;
    cfg.particle.end_size_min = 2.0f;
    cfg.particle.end_size_max = 4.0f;

    /* Golden color */
    cfg.particle.start_color = (Agentite_Color){1.0f, 0.85f, 0.3f, 1.0f};
    cfg.particle.end_color = (Agentite_Color){1.0f, 0.5f, 0.1f, 0.0f};

    cfg.blend = AGENTITE_BLEND_ADDITIVE;

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_texture(emitter, s_particle_texture);
        agentite_particle_emitter_set_position(emitter, x, y);
        agentite_particle_emitter_burst(emitter, 0);  /* Trigger burst */
    }
    return emitter;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure and initialize engine */
    Agentite_Config config = {
        .window_title = "Agentite - Particle System Example",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize sprite renderer */
    Agentite_SpriteRenderer *sprites = agentite_sprite_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );
    if (!sprites) {
        fprintf(stderr, "Failed to initialize sprite renderer\n");
        agentite_shutdown(engine);
        return 1;
    }

    /* Create particle texture */
    s_particle_texture = create_particle_texture(sprites);
    if (!s_particle_texture) {
        fprintf(stderr, "Failed to create particle texture\n");
    }

    /* Initialize input */
    Agentite_Input *input = agentite_input_init();

    /* Initialize text renderer for UI */
    Agentite_TextRenderer *text_renderer = agentite_text_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );
    Agentite_Font *font = NULL;
    if (text_renderer) {
        font = agentite_font_load(text_renderer, "assets/fonts/Roboto-Regular.ttf", 18);
        if (!font) {
            fprintf(stderr, "Failed to load font: %s\n", agentite_get_last_error());
        }
    } else {
        fprintf(stderr, "Failed to initialize text renderer\n");
    }

    /* Create particle system */
    Agentite_ParticleSystemConfig ps_config = AGENTITE_PARTICLE_SYSTEM_DEFAULT;
    ps_config.max_particles = 20000;  /* Higher limit for weather effects */
    Agentite_ParticleSystem *particles = agentite_particle_system_create(&ps_config);
    if (!particles) {
        fprintf(stderr, "Failed to create particle system\n");
        agentite_shutdown(engine);
        return 1;
    }

    printf("Particle System Example\n");
    printf("=======================\n");
    printf("1 - Explosion    2 - Fire        3 - Smoke\n");
    printf("4 - Sparks       5 - Rain        6 - Snow\n");
    printf("7 - Custom       8 - Trail       9 - Rectangle\n");
    printf("SPACE - Clear    R - Reset       ESC - Quit\n");
    printf("\nClick anywhere to spawn effects at mouse position.\n");

    /* Main loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        /* Process input */
        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
            /* Spawn explosion on left mouse click */
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                float click_x = event.button.x;
                float click_y = event.button.y;
                Agentite_ParticleEmitter *e = agentite_particle_preset_explosion(
                    particles, click_x, click_y,
                    (Agentite_Color){1.0f, 0.4f, 0.1f, 1.0f},
                    1.0f
                );
                if (e) {
                    agentite_particle_emitter_set_texture(e, s_particle_texture);
                    agentite_particle_emitter_burst(e, 0);
                }
            }
        }
        agentite_input_update(input);

        /* Get mouse position for spawning effects */
        float mx, my;
        agentite_input_get_mouse_position(input, &mx, &my);

        /* Handle key presses to spawn different effects */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_1)) {
            /* Explosion at mouse position */
            Agentite_ParticleEmitter *e = agentite_particle_preset_explosion(
                particles, mx, my,
                (Agentite_Color){1.0f, 0.6f, 0.2f, 1.0f},  /* Orange */
                1.5f  /* Scale */
            );
            if (e) {
                agentite_particle_emitter_set_texture(e, s_particle_texture);
                agentite_particle_emitter_burst(e, 0);
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_2)) {
            /* Fire at mouse position */
            Agentite_ParticleEmitter *e = agentite_particle_preset_fire(particles, mx, my, 1.0f);
            if (e) {
                agentite_particle_emitter_set_texture(e, s_particle_texture);
                agentite_particle_emitter_start(e);
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_3)) {
            /* Smoke at mouse position */
            Agentite_ParticleEmitter *e = agentite_particle_preset_smoke(particles, mx, my, 30.0f);
            if (e) {
                agentite_particle_emitter_set_texture(e, s_particle_texture);
                agentite_particle_emitter_start(e);
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_4)) {
            /* Sparks at mouse position */
            Agentite_ParticleEmitter *e = agentite_particle_preset_sparks(
                particles, mx, my,
                (Agentite_Color){1.0f, 0.9f, 0.5f, 1.0f}  /* Yellow-white */
            );
            if (e) {
                agentite_particle_emitter_set_texture(e, s_particle_texture);
                agentite_particle_emitter_burst(e, 30);
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_5)) {
            /* Toggle rain */
            if (s_rain_emitter) {
                agentite_particle_emitter_destroy(s_rain_emitter);
                s_rain_emitter = NULL;
                printf("Rain: OFF\n");
            } else {
                s_rain_emitter = agentite_particle_preset_rain(
                    particles,
                    WINDOW_WIDTH / 2.0f, 0,  /* Top center */
                    (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT,
                    0.7f  /* Intensity */
                );
                if (s_rain_emitter) {
                    agentite_particle_emitter_set_texture(s_rain_emitter, s_particle_texture);
                    agentite_particle_emitter_start(s_rain_emitter);
                    printf("Rain: ON (emitter created and started)\n");
                } else {
                    printf("Rain: FAILED to create emitter\n");
                }
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_6)) {
            /* Toggle snow */
            if (s_snow_emitter) {
                agentite_particle_emitter_destroy(s_snow_emitter);
                s_snow_emitter = NULL;
            } else {
                s_snow_emitter = agentite_particle_preset_snow(
                    particles,
                    WINDOW_WIDTH / 2.0f, 0,
                    (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT,
                    0.5f  /* Intensity */
                );
                if (s_snow_emitter) {
                    agentite_particle_emitter_set_texture(s_snow_emitter, s_particle_texture);
                    agentite_particle_emitter_start(s_snow_emitter);
                }
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_7)) {
            /* Custom emitter at mouse position */
            create_custom_emitter(particles, mx, my);
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_8)) {
            /* Toggle trail emitter */
            if (s_trail_emitter) {
                agentite_particle_emitter_destroy(s_trail_emitter);
                s_trail_emitter = NULL;
            } else {
                s_trail_emitter = agentite_particle_preset_trail(
                    particles,
                    (Agentite_Color){0.4f, 0.8f, 1.0f, 1.0f},  /* Cyan */
                    12.0f
                );
                if (s_trail_emitter) {
                    agentite_particle_emitter_set_texture(s_trail_emitter, s_particle_texture);
                    agentite_particle_emitter_start(s_trail_emitter);
                }
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_9)) {
            /* Rectangle emitter burst */
            create_rectangle_emitter(particles, mx, my);
        }

        /* Update trail emitter position to follow mouse */
        if (s_trail_emitter) {
            agentite_particle_emitter_set_position(s_trail_emitter, mx, my);
        }

        /* Clear all particles with SPACE */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
            agentite_particle_system_clear(particles);
        }

        /* Reset everything with R */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_R)) {
            /* Clear references to managed emitters */
            s_rain_emitter = NULL;
            s_snow_emitter = NULL;
            s_trail_emitter = NULL;

            /* Destroy and recreate particle system */
            agentite_particle_system_destroy(particles);
            particles = agentite_particle_system_create(&ps_config);
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(engine);
        }

        /* Note: Mouse click spawning is handled in event loop above */

        /* Update particle system */
        agentite_particle_system_update(particles, dt);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Build sprite batch with particles */
            agentite_sprite_begin(sprites, NULL);
            agentite_particle_system_draw(particles, sprites);
            agentite_sprite_upload(sprites, cmd);

            /* Build text batch */
            if (text_renderer && font) {
                agentite_text_begin(text_renderer);

                /* Draw instructions */
                agentite_text_draw_colored(text_renderer, font,
                    "1-9: Spawn effects  SPACE: Clear  R: Reset  Click: Explosion",
                    10, 10,
                    1.0f, 1.0f, 1.0f, 0.8f);

                /* Draw particle count */
                char stats[128];
                snprintf(stats, sizeof(stats), "Particles: %u / %u",
                    agentite_particle_system_get_count(particles),
                    agentite_particle_system_get_capacity(particles));
                agentite_text_draw_colored(text_renderer, font,
                    stats, 10, 30,
                    0.8f, 1.0f, 0.8f, 1.0f);

                /* Show active weather effects */
                char status[128];
                snprintf(status, sizeof(status), "Rain: %s  Snow: %s  Trail: %s",
                    s_rain_emitter ? "ON" : "OFF",
                    s_snow_emitter ? "ON" : "OFF",
                    s_trail_emitter ? "ON" : "OFF");
                agentite_text_draw_colored(text_renderer, font,
                    status, 10, 50,
                    0.8f, 0.8f, 1.0f, 1.0f);

                /* Draw controls at bottom of screen */
                agentite_text_draw_colored(text_renderer, font,
                    "1: Explosion  2: Fire  3: Smoke  4: Sparks  5: Rain  6: Snow  7: Custom  8: Trail  9: Rectangle",
                    10, WINDOW_HEIGHT - 50,
                    0.7f, 0.7f, 0.7f, 0.9f);
                agentite_text_draw_colored(text_renderer, font,
                    "SPACE: Clear all particles    R: Reset emitters    ESC: Quit    Click: Spawn explosion",
                    10, WINDOW_HEIGHT - 30,
                    0.7f, 0.7f, 0.7f, 0.9f);

                agentite_text_end(text_renderer);
                agentite_text_upload(text_renderer, cmd);
            }

            /* Render pass */
            if (agentite_begin_render_pass(engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                agentite_sprite_render(sprites, cmd, pass);
                if (text_renderer && font) {
                    agentite_text_render(text_renderer, cmd, pass);
                }
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_particle_system_destroy(particles);
    if (s_particle_texture) agentite_texture_destroy(sprites, s_particle_texture);
    if (font) agentite_font_destroy(text_renderer, font);
    if (text_renderer) agentite_text_shutdown(text_renderer);
    agentite_input_shutdown(input);
    agentite_sprite_shutdown(sprites);
    agentite_shutdown(engine);

    return 0;
}
