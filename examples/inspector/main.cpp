/**
 * Agentite Engine - Entity Inspector Example
 *
 * Demonstrates the ECS Entity Inspector tool:
 * - Runtime inspection of entities and their components
 * - Field-level display using the reflection system
 * - Entity selection and filtering
 * - Scrollable panels with automatic refresh
 *
 * This example creates various entities with different components
 * and shows them in a debug inspector panel.
 */

#include "agentite/agentite.h"
#include "agentite/ecs.h"
#include "agentite/ecs_reflect.h"
#include "agentite/ecs_inspector.h"
#include "agentite/ui.h"
#include "agentite/sprite.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Include game components for reflection registration */
#include "../../src/game/components.h"

/* Simple movement system to make entities animate */
static void MovementSystem(ecs_iter_t *it)
{
    C_Position *pos = ecs_field(it, C_Position, 0);
    C_Velocity *vel = ecs_field(it, C_Velocity, 1);

    for (int i = 0; i < it->count; i++) {
        pos[i].x += vel[i].vx * it->delta_time;
        pos[i].y += vel[i].vy * it->delta_time;

        /* Bounce off screen edges */
        if (pos[i].x < 50 || pos[i].x > 500) vel[i].vx = -vel[i].vx;
        if (pos[i].y < 50 || pos[i].y > 500) vel[i].vy = -vel[i].vy;
    }
}

/* Update health timer for demo */
static void HealthRegenSystem(ecs_iter_t *it)
{
    C_Health *health = ecs_field(it, C_Health, 0);

    for (int i = 0; i < it->count; i++) {
        /* Slowly regenerate health */
        if (health[i].health < health[i].max_health) {
            health[i].health += 1;
            if (health[i].health > health[i].max_health) {
                health[i].health = health[i].max_health;
            }
        }
    }
}

/* Create a circle texture for entities */
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
                float alpha = 1.0f - (dist / radius) * 0.3f;
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

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Initialize engine */
    Agentite_Config config = AGENTITE_DEFAULT_CONFIG;
    config.window_title = "Agentite - Entity Inspector Example";
    config.window_width = 1280;
    config.window_height = 720;

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    SDL_GPUDevice *gpu = agentite_get_gpu_device(engine);
    SDL_Window *window = agentite_get_window(engine);

    /* Initialize sprite renderer for visualizing entities */
    Agentite_SpriteRenderer *sprites = agentite_sprite_init(gpu, window);
    Agentite_Texture *circle_tex = create_circle_texture(sprites, 32);

    /* Initialize UI system */
    AUI_Context *ui = aui_init(gpu, window,
                               config.window_width, config.window_height,
                               "assets/fonts/Roboto-Regular.ttf", 14.0f);
    if (!ui) {
        /* Try fallback font */
        ui = aui_init(gpu, window,
                      config.window_width, config.window_height,
                      "/System/Library/Fonts/Helvetica.ttc", 14.0f);
    }
    if (!ui) {
        fprintf(stderr, "Failed to initialize UI\n");
        agentite_sprite_shutdown(sprites);
        agentite_shutdown(engine);
        return 1;
    }

    /* Set DPI scale */
    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    /* Initialize ECS */
    Agentite_World *ecs_wrapper = agentite_ecs_init();
    ecs_world_t *world = agentite_ecs_get_world(ecs_wrapper);

    /* Register built-in components */
    agentite_ecs_register_components(ecs_wrapper);

    /* Register game-specific components */
    game_components_register(world);

    /* Create reflection registry */
    Agentite_ReflectRegistry *registry = agentite_reflect_create();

    /* Register all component reflection data */
    game_components_register_reflection(world, registry);

    /* Register systems */
    ECS_SYSTEM(world, MovementSystem, EcsOnUpdate, C_Position, C_Velocity);
    ECS_SYSTEM(world, HealthRegenSystem, EcsPostUpdate, C_Health);

    /* Create the inspector */
    Agentite_InspectorConfig inspector_config = AGENTITE_INSPECTOR_CONFIG_DEFAULT;
    inspector_config.entity_list_width = 220;
    inspector_config.show_entity_ids = true;
    inspector_config.show_component_sizes = true;

    Agentite_Inspector *inspector = agentite_inspector_create(
        ecs_wrapper, registry, &inspector_config);

    if (!inspector) {
        fprintf(stderr, "Failed to create inspector\n");
        agentite_reflect_destroy(registry);
        agentite_ecs_shutdown(ecs_wrapper);
        aui_shutdown(ui);
        agentite_sprite_shutdown(sprites);
        agentite_shutdown(engine);
        return 1;
    }

    /* Create sample entities */

    /* Player entity */
    ecs_entity_t player = agentite_ecs_entity_new_named(ecs_wrapper, "Player");
    {
        C_Position pos = {150, 300};
        C_Velocity vel = {80, 50};
        C_Size size = {8, 8};
        C_Color col = {0.2f, 0.8f, 0.3f, 1.0f};
        C_Health hp = {80, 100};
        C_Player plr = {0};
        C_Speed spd = {150.0f, 300.0f, 0.9f};
        ecs_set_id(world, player, ecs_id(C_Position), sizeof(C_Position), &pos);
        ecs_set_id(world, player, ecs_id(C_Velocity), sizeof(C_Velocity), &vel);
        ecs_set_id(world, player, ecs_id(C_Size), sizeof(C_Size), &size);
        ecs_set_id(world, player, ecs_id(C_Color), sizeof(C_Color), &col);
        ecs_set_id(world, player, ecs_id(C_Health), sizeof(C_Health), &hp);
        ecs_set_id(world, player, ecs_id(C_Player), sizeof(C_Player), &plr);
        ecs_set_id(world, player, ecs_id(C_Speed), sizeof(C_Speed), &spd);
    }

    /* Enemy entities - create 15 for scrollbar testing */
    for (int i = 0; i < 15; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Enemy_%d", i + 1);
        ecs_entity_t enemy = agentite_ecs_entity_new_named(ecs_wrapper, name);
        float angle = (float)i * 0.4f;
        C_Position pos = {250.0f + cosf(angle) * 150.0f, 300.0f + sinf(angle) * 150.0f};
        C_Velocity vel = {-40.0f + sinf(angle) * 30.0f, 30.0f + cosf(angle) * 30.0f};
        C_Size size = {5.0f + (i % 3) * 1.0f, 5.0f + (i % 3) * 1.0f};
        C_Color col = {0.9f - (i % 5) * 0.1f, 0.2f + (i % 3) * 0.1f, 0.2f, 1.0f};
        C_Health hp = {30 + i * 5, 50 + i * 5};
        C_Enemy enm = {i % 3, 150.0f + i * 10.0f};
        C_AIState ai = {i % 4, (float)(i % 10) * 0.5f, player};
        ecs_set_id(world, enemy, ecs_id(C_Position), sizeof(C_Position), &pos);
        ecs_set_id(world, enemy, ecs_id(C_Velocity), sizeof(C_Velocity), &vel);
        ecs_set_id(world, enemy, ecs_id(C_Size), sizeof(C_Size), &size);
        ecs_set_id(world, enemy, ecs_id(C_Color), sizeof(C_Color), &col);
        ecs_set_id(world, enemy, ecs_id(C_Health), sizeof(C_Health), &hp);
        ecs_set_id(world, enemy, ecs_id(C_Enemy), sizeof(C_Enemy), &enm);
        ecs_set_id(world, enemy, ecs_id(C_AIState), sizeof(C_AIState), &ai);
    }

    /* Projectile entities - create 10 for variety */
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Projectile_%d", i + 1);
        ecs_entity_t proj = agentite_ecs_entity_new_named(ecs_wrapper, name);
        float angle = (float)i * 0.6f;
        C_Position pos = {350.0f + cosf(angle) * 50.0f, 350.0f + sinf(angle) * 50.0f};
        C_Velocity vel = {cosf(angle) * 100.0f, sinf(angle) * 100.0f};
        C_Size size = {3.0f, 3.0f};
        C_Color col = {1.0f, 0.6f + (i % 4) * 0.1f, 0.1f + (i % 3) * 0.1f, 1.0f};
        C_Projectile prj = {player, 2.0f + (float)i * 0.3f, 5.0f};
        C_Damage dmg = {15 + i * 3, i % 3};
        ecs_set_id(world, proj, ecs_id(C_Position), sizeof(C_Position), &pos);
        ecs_set_id(world, proj, ecs_id(C_Velocity), sizeof(C_Velocity), &vel);
        ecs_set_id(world, proj, ecs_id(C_Size), sizeof(C_Size), &size);
        ecs_set_id(world, proj, ecs_id(C_Color), sizeof(C_Color), &col);
        ecs_set_id(world, proj, ecs_id(C_Projectile), sizeof(C_Projectile), &prj);
        ecs_set_id(world, proj, ecs_id(C_Damage), sizeof(C_Damage), &dmg);
    }

    /* Additional obstacles */
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Wall_%d", i + 1);
        ecs_entity_t wall = agentite_ecs_entity_new_named(ecs_wrapper, name);
        C_Position pos = {100.0f + i * 80.0f, 450.0f};
        C_Size size = {10, 10};
        C_Color col = {0.4f + (i % 2) * 0.2f, 0.4f + (i % 2) * 0.2f, 0.5f, 1.0f};
        C_Collider cld = {0, 0, 10, 10, true, false};
        ecs_set_id(world, wall, ecs_id(C_Position), sizeof(C_Position), &pos);
        ecs_set_id(world, wall, ecs_id(C_Size), sizeof(C_Size), &size);
        ecs_set_id(world, wall, ecs_id(C_Color), sizeof(C_Color), &col);
        ecs_set_id(world, wall, ecs_id(C_Collider), sizeof(C_Collider), &cld);
    }

    /* Static obstacle */
    ecs_entity_t obstacle = agentite_ecs_entity_new_named(ecs_wrapper, "Obstacle");
    {
        C_Position pos = {350, 350};
        C_Size size = {12, 12};
        C_Color col = {0.5f, 0.5f, 0.5f, 1.0f};
        C_Collider cld = {0, 0, 12, 12, true, false};
        ecs_set_id(world, obstacle, ecs_id(C_Position), sizeof(C_Position), &pos);
        ecs_set_id(world, obstacle, ecs_id(C_Size), sizeof(C_Size), &size);
        ecs_set_id(world, obstacle, ecs_id(C_Color), sizeof(C_Color), &col);
        ecs_set_id(world, obstacle, ecs_id(C_Collider), sizeof(C_Collider), &cld);
    }

    printf("Entity Inspector Example\n");
    printf("========================\n");
    printf("Created %d sample entities:\n", 1 + 15 + 10 + 5 + 1);
    printf("  - 1 Player with position, velocity, health, speed\n");
    printf("  - 15 Enemies with AI state and health\n");
    printf("  - 10 Projectiles with damage\n");
    printf("  - 5 Walls with colliders\n");
    printf("  - 1 Static obstacle\n\n");
    printf("Use the inspector panel on the right to:\n");
    printf("  - Browse all entities\n");
    printf("  - Select entities to view their components\n");
    printf("  - See component field values in real-time\n\n");

    /* Main loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        /* Poll events */
        aui_begin_frame(ui, dt);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (aui_process_event(ui, &event)) {
                continue;
            }
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
            /* Toggle inspector with Tab */
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_TAB) {
                /* Could toggle inspector visibility here */
            }
        }

        /* Progress ECS - runs movement system */
        agentite_ecs_progress(ecs_wrapper, dt);

        /* Acquire command buffer */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);

        /* Draw entities as circles */
        agentite_sprite_begin(sprites, cmd);

        /* Query for all entities with position and color */
        ecs_query_desc_t query_desc = {0};
        query_desc.terms[0].id = ecs_id(C_Position);
        query_desc.terms[1].id = ecs_id(C_Color);
        query_desc.terms[2].id = ecs_id(C_Size);
        query_desc.terms[2].oper = EcsOptional;
        ecs_query_t *query = ecs_query_init(world, &query_desc);

        ecs_iter_t it = ecs_query_iter(world, query);
        while (ecs_query_next(&it)) {
            C_Position *pos = ecs_field(&it, C_Position, 0);
            C_Color *col = ecs_field(&it, C_Color, 1);
            C_Size *size = ecs_field(&it, C_Size, 2);

            for (int i = 0; i < it.count; i++) {
                float w = size ? size[i].width : 16.0f;
                float h = size ? size[i].height : 16.0f;

                Agentite_Sprite sprite = agentite_sprite_from_texture(circle_tex);
                agentite_sprite_draw_full(sprites, &sprite,
                                         pos[i].x, pos[i].y,
                                         w, h, 0, 0.5f, 0.5f,
                                         col[i].r, col[i].g, col[i].b, col[i].a);

                /* Highlight selected entity */
                if (it.entities[i] == agentite_inspector_get_selected(inspector)) {
                    agentite_sprite_draw_full(sprites, &sprite,
                                             pos[i].x, pos[i].y,
                                             w + 8, h + 8, 0, 0.5f, 0.5f,
                                             1.0f, 1.0f, 1.0f, 0.3f);
                }
            }
        }
        ecs_query_fini(query);

        agentite_sprite_upload(sprites, cmd);

        /* Draw Inspector UI */
        float inspector_x = config.window_width - 520;
        float inspector_y = 10;
        float inspector_w = 500;
        float inspector_h = config.window_height - 20;
        agentite_inspector_draw(inspector, ui, inspector_x, inspector_y,
                                 inspector_w, inspector_h);

        /* Draw info panel */
        if (aui_begin_panel(ui, "Info", 10, 10, 200, 100, AUI_PANEL_TITLE_BAR)) {
            char fps_buf[32];
            snprintf(fps_buf, sizeof(fps_buf), "FPS: %.0f", 1.0f / dt);
            aui_label(ui, fps_buf);

            ecs_entity_t sel = agentite_inspector_get_selected(inspector);
            if (sel) {
                char sel_buf[64];
                const char *name = ecs_get_name(world, sel);
                if (name) {
                    snprintf(sel_buf, sizeof(sel_buf), "Selected: %s", name);
                } else {
                    snprintf(sel_buf, sizeof(sel_buf), "Selected: %llu",
                             (unsigned long long)sel);
                }
                aui_label(ui, sel_buf);
            } else {
                aui_label(ui, "Selected: none");
            }

            aui_end_panel(ui);
        }

        aui_end_frame(ui);
        aui_upload(ui, cmd);

        /* Render */
        if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
            agentite_sprite_render(sprites, cmd, agentite_get_render_pass(engine));
            aui_render(ui, cmd, agentite_get_render_pass(engine));
            agentite_end_render_pass(engine);
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_inspector_destroy(inspector);
    agentite_reflect_destroy(registry);
    agentite_texture_destroy(sprites, circle_tex);
    aui_shutdown(ui);
    agentite_sprite_shutdown(sprites);
    agentite_ecs_shutdown(ecs_wrapper);
    agentite_shutdown(engine);

    return 0;
}
