/**
 * Agentite Engine - ECS Custom System Example
 *
 * Deep dive into the Entity Component System (Flecs) patterns:
 * - Defining custom components
 * - Creating systems with queries
 * - System ordering and phases
 * - Entity relationships and hierarchies
 * - Component lifecycle (add/remove)
 *
 * This example creates a simple particle simulation where:
 * - Emitters spawn particles
 * - Particles have velocity and lifetime
 * - Physics system moves particles
 * - Lifetime system removes expired particles
 * - Render system draws everything
 */

#include "agentite/agentite.h"
#include "agentite/ecs.h"
#include "agentite/sprite.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * Custom Components
 *
 * Components are pure data structs. Define them with ECS_COMPONENT_DECLARE
 * to make them available for system queries.
 *============================================================================*/

/* Particle emitter - spawns particles periodically */
typedef struct {
    float spawn_rate;       /* Particles per second */
    float spawn_timer;      /* Time accumulator */
    float particle_speed;   /* Initial particle speed */
    float spread_angle;     /* Spread angle in degrees */
    float direction;        /* Base direction in degrees */
} C_Emitter;

/* Lifetime component - entity is deleted when lifetime <= 0 */
typedef struct {
    float remaining;        /* Seconds remaining */
    float initial;          /* Initial lifetime (for effects) */
} C_Lifetime;

/* Particle tag - marks an entity as a particle */
typedef struct {
    ecs_entity_t emitter;   /* Parent emitter entity */
} C_Particle;

/* Gravity affected component */
typedef struct {
    float strength;         /* Gravity multiplier */
} C_GravityAffected;

/* Declare component IDs */
ECS_COMPONENT_DECLARE(C_Emitter);
ECS_COMPONENT_DECLARE(C_Lifetime);
ECS_COMPONENT_DECLARE(C_Particle);
ECS_COMPONENT_DECLARE(C_GravityAffected);

/*============================================================================
 * Systems
 *
 * Systems are functions that operate on entities matching a query.
 * They run automatically during ecs_progress() based on their phase.
 *============================================================================*/

/* Global state for rendering (passed via world context) */
typedef struct {
    Agentite_SpriteRenderer *sprites;
    Agentite_Texture *particle_tex;
    int particle_count;
    int emitter_count;
} RenderContext;

/* Physics System: Move entities based on velocity (EcsOnUpdate) */
static void PhysicsSystem(ecs_iter_t *it)
{
    C_Position *pos = ecs_field(it, C_Position, 0);
    C_Velocity *vel = ecs_field(it, C_Velocity, 1);

    for (int i = 0; i < it->count; i++) {
        pos[i].x += vel[i].vx * it->delta_time;
        pos[i].y += vel[i].vy * it->delta_time;
    }
}

/* Gravity System: Apply gravity to affected entities (EcsOnUpdate) */
static void GravitySystem(ecs_iter_t *it)
{
    C_Velocity *vel = ecs_field(it, C_Velocity, 0);
    C_GravityAffected *grav = ecs_field(it, C_GravityAffected, 1);

    for (int i = 0; i < it->count; i++) {
        vel[i].vy += 200.0f * grav[i].strength * it->delta_time;
    }
}

/* Lifetime System: Count down and delete expired entities (EcsPostUpdate) */
static void LifetimeSystem(ecs_iter_t *it)
{
    C_Lifetime *life = ecs_field(it, C_Lifetime, 0);

    for (int i = 0; i < it->count; i++) {
        life[i].remaining -= it->delta_time;
        if (life[i].remaining <= 0) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

/* Emitter System: Spawn particles (EcsOnUpdate) */
static void EmitterSystem(ecs_iter_t *it)
{
    C_Emitter *emitter = ecs_field(it, C_Emitter, 0);
    C_Position *pos = ecs_field(it, C_Position, 1);

    for (int i = 0; i < it->count; i++) {
        emitter[i].spawn_timer += it->delta_time;

        float spawn_interval = 1.0f / emitter[i].spawn_rate;
        while (emitter[i].spawn_timer >= spawn_interval) {
            emitter[i].spawn_timer -= spawn_interval;

            /* Calculate random direction within spread */
            float angle_deg = emitter[i].direction +
                             ((float)rand() / RAND_MAX - 0.5f) * emitter[i].spread_angle;
            float angle_rad = angle_deg * 3.14159f / 180.0f;

            /* Random speed variation */
            float speed = emitter[i].particle_speed * (0.8f + (float)rand() / RAND_MAX * 0.4f);

            /* Create particle entity */
            ecs_entity_t particle = ecs_new(it->world);

            /* Add components */
            ecs_set(it->world, particle, C_Position, {pos[i].x, pos[i].y});
            ecs_set(it->world, particle, C_Velocity, {
                cosf(angle_rad) * speed,
                sinf(angle_rad) * speed
            });
            ecs_set(it->world, particle, C_Lifetime, {
                2.0f + (float)rand() / RAND_MAX * 1.0f,  /* 2-3 second lifetime */
                3.0f
            });
            ecs_set(it->world, particle, C_Particle, {it->entities[i]});
            ecs_set(it->world, particle, C_GravityAffected, {1.0f});
            ecs_set(it->world, particle, C_Color, {
                0.9f + (float)rand() / RAND_MAX * 0.1f,
                0.5f + (float)rand() / RAND_MAX * 0.3f,
                0.1f,
                1.0f
            });
        }
    }
}

/* Render System: Draw all particles (custom, called manually) */
static void render_particles(ecs_world_t *world, RenderContext *ctx)
{
    /* Query for all particles with position and color */
    ecs_query_t *q = ecs_query(world, {
        .terms = {
            { .id = ecs_id(C_Position) },
            { .id = ecs_id(C_Color) },
            { .id = ecs_id(C_Lifetime) }
        }
    });

    ctx->particle_count = 0;

    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        C_Position *pos = ecs_field(&it, C_Position, 0);
        C_Color *col = ecs_field(&it, C_Color, 1);
        C_Lifetime *life = ecs_field(&it, C_Lifetime, 2);

        for (int i = 0; i < it.count; i++) {
            /* Fade out based on remaining lifetime */
            float alpha = life[i].remaining / life[i].initial;
            if (alpha > 1.0f) alpha = 1.0f;

            Agentite_Sprite sprite = agentite_sprite_from_texture(ctx->particle_tex);
            agentite_sprite_draw_full(ctx->sprites, &sprite,
                                     pos[i].x, pos[i].y,
                                     8, 8, 0, 0.5f, 0.5f,
                                     col[i].r, col[i].g, col[i].b, col[i].a * alpha);
            ctx->particle_count++;
        }
    }

    ecs_query_fini(q);
}

/* Render emitters (as larger circles) */
static void render_emitters(ecs_world_t *world, RenderContext *ctx)
{
    ecs_query_t *q = ecs_query(world, {
        .terms = {
            { .id = ecs_id(C_Emitter) },
            { .id = ecs_id(C_Position) }
        }
    });

    ctx->emitter_count = 0;

    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        C_Position *pos = ecs_field(&it, C_Position, 1);

        for (int i = 0; i < it.count; i++) {
            Agentite_Sprite sprite = agentite_sprite_from_texture(ctx->particle_tex);
            agentite_sprite_draw_full(ctx->sprites, &sprite,
                                     pos[i].x, pos[i].y,
                                     16, 16, 0, 0.5f, 0.5f,
                                     0.2f, 0.6f, 1.0f, 1.0f);
            ctx->emitter_count++;
        }
    }

    ecs_query_fini(q);
}

/* Create a white circle texture */
static Agentite_Texture *create_circle_texture(Agentite_SpriteRenderer *sr, int size)
{
    unsigned char *pixels = (unsigned char *)calloc(size * size * 4, 1);
    float center = size / 2.0f;
    float radius = size / 2.0f - 1;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x - center;
            float dy = y - center;
            float dist = sqrtf(dx * dx + dy * dy);

            int idx = (y * size + x) * 4;
            if (dist <= radius) {
                float alpha = 1.0f - (dist / radius) * 0.5f;  /* Soft edge */
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = (unsigned char)(alpha * 255);
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Initialize engine */
    Agentite_Config config = AGENTITE_DEFAULT_CONFIG;
    config.window_title = "Agentite - ECS Custom System Example";
    config.window_width = 1024;
    config.window_height = 768;

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    SDL_GPUDevice *gpu = agentite_get_gpu_device(engine);
    SDL_Window *window = agentite_get_window(engine);

    /* Initialize sprite renderer */
    Agentite_SpriteRenderer *sprites = agentite_sprite_init(gpu, window);
    Agentite_TextRenderer *text = agentite_text_init(gpu, window);

    /* Try to load a font for HUD */
    Agentite_Font *font = agentite_font_load(text, "assets/fonts/NotoSans-Regular.ttf", 16);
    if (!font) {
        font = agentite_font_load(text, "/System/Library/Fonts/Helvetica.ttc", 16);
    }

    /* Create particle texture */
    Agentite_Texture *particle_tex = create_circle_texture(sprites, 16);

    /* Initialize ECS */
    Agentite_World *ecs_wrapper = agentite_ecs_init();
    ecs_world_t *world = agentite_ecs_get_world(ecs_wrapper);

    /* Register built-in components */
    agentite_ecs_register_components(ecs_wrapper);

    /* Register custom components */
    ECS_COMPONENT_DEFINE(world, C_Emitter);
    ECS_COMPONENT_DEFINE(world, C_Lifetime);
    ECS_COMPONENT_DEFINE(world, C_Particle);
    ECS_COMPONENT_DEFINE(world, C_GravityAffected);

    /* Register systems with phases */
    /* EcsOnUpdate: Main game logic */
    ECS_SYSTEM(world, EmitterSystem, EcsOnUpdate, C_Emitter, C_Position);
    ECS_SYSTEM(world, PhysicsSystem, EcsOnUpdate, C_Position, C_Velocity);
    ECS_SYSTEM(world, GravitySystem, EcsOnUpdate, C_Velocity, C_GravityAffected);

    /* EcsPostUpdate: Cleanup (runs after main update) */
    ECS_SYSTEM(world, LifetimeSystem, EcsPostUpdate, C_Lifetime);

    /* Create emitters at different positions */
    ecs_entity_t emitter1 = ecs_new(world);
    ecs_set(world, emitter1, C_Position, {300, 600});
    ecs_set(world, emitter1, C_Emitter, {
        .spawn_rate = 50.0f,
        .particle_speed = 150.0f,
        .spread_angle = 60.0f,
        .direction = -90.0f  /* Up */
    });

    ecs_entity_t emitter2 = ecs_new(world);
    ecs_set(world, emitter2, C_Position, {700, 600});
    ecs_set(world, emitter2, C_Emitter, {
        .spawn_rate = 30.0f,
        .particle_speed = 200.0f,
        .spread_angle = 45.0f,
        .direction = -90.0f  /* Up */
    });

    ecs_entity_t emitter3 = ecs_new(world);
    ecs_set(world, emitter3, C_Position, {500, 400});
    ecs_set(world, emitter3, C_Emitter, {
        .spawn_rate = 20.0f,
        .particle_speed = 100.0f,
        .spread_angle = 360.0f,  /* All directions */
        .direction = 0.0f
    });

    /* Render context */
    RenderContext render_ctx = {
        .sprites = sprites,
        .particle_tex = particle_tex,
        .particle_count = 0,
        .emitter_count = 0
    };

    printf("ECS Custom System Example\n");
    printf("=========================\n");
    printf("Systems:\n");
    printf("  - EmitterSystem (EcsOnUpdate): Spawns particles\n");
    printf("  - PhysicsSystem (EcsOnUpdate): Moves entities\n");
    printf("  - GravitySystem (EcsOnUpdate): Applies gravity\n");
    printf("  - LifetimeSystem (EcsPostUpdate): Removes expired entities\n\n");

    /* Main loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        /* Poll events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }

        /* Progress ECS - runs all systems */
        agentite_ecs_progress(ecs_wrapper, dt);

        /* Acquire command buffer for rendering */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);

        /* Begin sprite batch */
        agentite_sprite_begin(sprites, cmd);
        render_emitters(world, &render_ctx);
        render_particles(world, &render_ctx);
        agentite_sprite_upload(sprites, cmd);

        /* Draw HUD */
        if (font) {
            agentite_text_begin(text);
            char buf[128];
            snprintf(buf, sizeof(buf), "Emitters: %d  |  Particles: %d  |  FPS: %.0f",
                    render_ctx.emitter_count, render_ctx.particle_count, 1.0f / dt);
            agentite_text_draw_colored(text, font, buf, 10, 10, 1, 1, 1, 1);
            agentite_text_draw_colored(text, font,
                "Systems: Emitter -> Physics -> Gravity -> Lifetime (automatic via ecs_progress)",
                10, 30, 0.7f, 0.7f, 0.7f, 1.0f);
            agentite_text_upload(text, cmd);
        }

        /* Render */
        if (agentite_begin_render_pass(engine, 0.05f, 0.05f, 0.1f, 1.0f)) {
            agentite_sprite_render(sprites, cmd, agentite_get_render_pass(engine));
            if (font) {
                agentite_text_render(text, cmd, agentite_get_render_pass(engine));
            }
            agentite_end_render_pass(engine);
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_texture_destroy(sprites, particle_tex);
    if (font) agentite_font_destroy(text, font);
    agentite_text_shutdown(text);
    agentite_sprite_shutdown(sprites);
    agentite_ecs_shutdown(ecs_wrapper);
    agentite_shutdown(engine);

    return 0;
}
