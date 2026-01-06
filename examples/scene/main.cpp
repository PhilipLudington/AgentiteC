/**
 * Agentite Engine - Scene Example
 *
 * Demonstrates loading complete levels from scene files and transitioning
 * between them. Each scene contains multiple entities with positions.
 *
 * Controls:
 *   1/2: Switch to Scene 1 or 2
 *   F: Find player entity by name
 *   I: Show scene info
 *   ESC: Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/text.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include "agentite/ecs.h"
#include "agentite/ecs_reflect.h"
#include "agentite/prefab.h"
#include "agentite/scene.h"
#include "agentite/transform.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Custom Game Components (same as prefab example for compatibility)
 * ============================================================================ */

typedef struct C_EnemyType {
    int type_id;
    float aggro_range;
} C_EnemyType;

typedef struct C_Item {
    int item_id;
    int value;
    bool can_pickup;
} C_Item;

typedef struct C_SpriteRef {
    int texture_id;
    float scale;
} C_SpriteRef;

/* Tag component for scene entities */
typedef struct C_SceneEntity {
    int dummy;  /* Tag component, no data needed */
} C_SceneEntity;

ECS_COMPONENT_DECLARE(C_EnemyType);
ECS_COMPONENT_DECLARE(C_Item);
ECS_COMPONENT_DECLARE(C_SpriteRef);
ECS_COMPONENT_DECLARE(C_SceneEntity);

/* ============================================================================
 * Helper: Create colored texture with pattern
 * ============================================================================ */

static Agentite_Texture *create_entity_texture(Agentite_SpriteRenderer *sr,
                                                 int size,
                                                 uint8_t r, uint8_t g, uint8_t b,
                                                 bool is_player) {
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            bool is_border = (x < 2 || x >= size - 2 || y < 2 || y >= size - 2);

            /* Players have a diamond highlight in center */
            int cx = x - size / 2;
            int cy = y - size / 2;
            bool is_highlight = is_player && (abs(cx) + abs(cy) < size / 4);

            if (is_highlight) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
            } else if (is_border) {
                pixels[idx + 0] = r / 2;
                pixels[idx + 1] = g / 2;
                pixels[idx + 2] = b / 2;
            } else {
                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
            }
            pixels[idx + 3] = 255;
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* ============================================================================
 * Register components for reflection
 * ============================================================================ */

static void register_game_components(ecs_world_t *world,
                                       Agentite_ReflectRegistry *reflect) {
    ECS_COMPONENT_DEFINE(world, C_EnemyType);
    ECS_COMPONENT_DEFINE(world, C_Item);
    ECS_COMPONENT_DEFINE(world, C_SpriteRef);
    ECS_COMPONENT_DEFINE(world, C_SceneEntity);

    AGENTITE_REFLECT_COMPONENT(reflect, world, C_EnemyType,
        AGENTITE_FIELD(C_EnemyType, type_id, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_EnemyType, aggro_range, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(reflect, world, C_Item,
        AGENTITE_FIELD(C_Item, item_id, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Item, value, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Item, can_pickup, AGENTITE_FIELD_BOOL)
    );

    AGENTITE_REFLECT_COMPONENT(reflect, world, C_SpriteRef,
        AGENTITE_FIELD(C_SpriteRef, texture_id, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_SpriteRef, scale, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(reflect, world, C_SceneEntity,
        AGENTITE_FIELD(C_SceneEntity, dummy, AGENTITE_FIELD_INT)
    );

    /* Standard components */
    AGENTITE_REFLECT_COMPONENT(reflect, world, C_Position,
        AGENTITE_FIELD(C_Position, x, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Position, y, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(reflect, world, C_Health,
        AGENTITE_FIELD(C_Health, health, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Health, max_health, AGENTITE_FIELD_INT)
    );

    AGENTITE_REFLECT_COMPONENT(reflect, world, C_Color,
        AGENTITE_FIELD(C_Color, r, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Color, g, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Color, b, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Color, a, AGENTITE_FIELD_FLOAT)
    );
}

/* ============================================================================
 * Print scene information
 * ============================================================================ */

static void print_scene_info(Agentite_Scene *scene, ecs_world_t *world) {
    if (!scene) {
        printf("No scene loaded\n");
        return;
    }

    printf("\n=== Scene Info ===\n");
    printf("  Name: %s\n", agentite_scene_get_name(scene));
    printf("  Path: %s\n", agentite_scene_get_path(scene));
    printf("  State: %d\n", agentite_scene_get_state(scene));
    printf("  Root entities: %zu\n", agentite_scene_get_root_count(scene));
    printf("  Total entities: %zu\n", agentite_scene_get_entity_count(scene));

    /* List entities */
    ecs_entity_t entities[64];
    size_t count = agentite_scene_get_entities(scene, entities, 64);
    printf("  Entities:\n");

    for (size_t i = 0; i < count; i++) {
        ecs_entity_t e = entities[i];
        const char *name = ecs_get_name(world, e);
        const C_Position *pos = ecs_get(world, e, C_Position);

        printf("    [%llu] %s", (unsigned long long)e, name ? name : "(unnamed)");
        if (pos) {
            printf(" at (%.0f, %.0f)", pos->x, pos->y);
        }
        printf("\n");
    }
    printf("==================\n\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Agentite Scene Example ===\n");
    printf("1/2: Load Scene 1 or 2\n");
    printf("F: Find player entity\n");
    printf("I: Show scene info\n");
    printf("ESC: Quit\n\n");

    /* Initialize engine */
    Agentite_Config config = {
        .window_title = "Agentite - Scene Example",
        .window_width = 1280,
        .window_height = 720,
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

    /* Initialize text renderer */
    Agentite_TextRenderer *text = agentite_text_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );

    /* Load font */
    Agentite_Font *font = agentite_font_load(text, "assets/fonts/Roboto-Regular.ttf", 16);
    if (!font) {
        font = agentite_font_load(text, "assets/fonts/NotoSans-Regular.ttf", 16);
    }

    /* Initialize camera - center it so world coords match screen coords */
    Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_sprite_set_camera(sprites, camera);
    agentite_camera_set_position(camera, 640.0f, 360.0f);
    agentite_camera_update(camera);

    /* Initialize input */
    Agentite_Input *input = agentite_input_init();

    /* Initialize ECS */
    Agentite_World *ecs_world = agentite_ecs_init();
    ecs_world_t *world = agentite_ecs_get_world(ecs_world);
    agentite_ecs_register_components(ecs_world);
    agentite_transform_register(world);

    /* Initialize reflection registry */
    Agentite_ReflectRegistry *reflect = agentite_reflect_create();
    register_game_components(world, reflect);

    /* Initialize prefab registry (scenes can reference prefabs) */
    Agentite_PrefabRegistry *prefabs = agentite_prefab_registry_create();

    /* Initialize scene manager */
    Agentite_SceneManager *scenes = agentite_scene_manager_create();

    /* Setup load context */
    Agentite_SceneLoadContext load_ctx = {
        .reflect = reflect,
        .assets = NULL,
        .prefabs = prefabs,
        .preload_assets = false
    };

    /* Create textures */
    Agentite_Texture *tex_player = create_entity_texture(sprites, 40, 80, 150, 255, true);
    Agentite_Texture *tex_enemy = create_entity_texture(sprites, 32, 255, 80, 80, false);
    Agentite_Texture *tex_item = create_entity_texture(sprites, 24, 255, 215, 0, false);
    Agentite_Texture *tex_platform = create_entity_texture(sprites, 64, 100, 100, 100, false);

    Agentite_Sprite sprite_player = agentite_sprite_from_texture(tex_player);
    Agentite_Sprite sprite_enemy = agentite_sprite_from_texture(tex_enemy);
    Agentite_Sprite sprite_item = agentite_sprite_from_texture(tex_item);
    Agentite_Sprite sprite_platform = agentite_sprite_from_texture(tex_platform);

    /* Info message display (shown for a few seconds after F/I press) */
    char info_message[256] = {0};
    float info_timer = 0.0f;

    /* Load initial scene */
    printf("Loading initial scene...\n");
    Agentite_Scene *current_scene = agentite_scene_transition(
        scenes,
        "examples/scene/scenes/level1.scene",
        world,
        &load_ctx
    );

    if (current_scene) {
        printf("Loaded scene: %s\n", agentite_scene_get_name(current_scene));
        print_scene_info(current_scene, world);
    } else {
        printf("Warning: Could not load level1.scene: %s\n",
               agentite_scene_get_error());
    }

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);

        /* Process input */
        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Scene switching */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_1)) {
            printf("\n--- Switching to Scene 1 ---\n");
            Agentite_Scene *new_scene = agentite_scene_transition(
                scenes,
                "examples/scene/scenes/level1.scene",
                world,
                &load_ctx
            );
            if (new_scene) {
                current_scene = new_scene;
                print_scene_info(current_scene, world);
            } else {
                printf("Failed to load scene 1: %s\n", agentite_scene_get_error());
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_2)) {
            printf("\n--- Switching to Scene 2 ---\n");
            Agentite_Scene *new_scene = agentite_scene_transition(
                scenes,
                "examples/scene/scenes/level2.scene",
                world,
                &load_ctx
            );
            if (new_scene) {
                current_scene = new_scene;
                print_scene_info(current_scene, world);
            } else {
                printf("Failed to load scene 2: %s\n", agentite_scene_get_error());
            }
        }

        /* Find player by name */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F)) {
            if (current_scene) {
                ecs_entity_t player = agentite_scene_find_entity(current_scene, "Player");
                if (player != 0) {
                    const C_Position *pos = ecs_get(world, player, C_Position);
                    if (pos) {
                        snprintf(info_message, sizeof(info_message),
                                 "Found Player at (%.0f, %.0f)", pos->x, pos->y);
                    } else {
                        snprintf(info_message, sizeof(info_message),
                                 "Found Player (no position)");
                    }
                } else {
                    snprintf(info_message, sizeof(info_message),
                             "Player not found in scene");
                }
            } else {
                snprintf(info_message, sizeof(info_message), "No scene loaded");
            }
            info_timer = 3.0f;
        }

        /* Show scene info */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_I)) {
            if (current_scene) {
                snprintf(info_message, sizeof(info_message),
                         "Scene '%s': %zu entities, %zu roots",
                         agentite_scene_get_name(current_scene),
                         agentite_scene_get_entity_count(current_scene),
                         agentite_scene_get_root_count(current_scene));
            } else {
                snprintf(info_message, sizeof(info_message), "No scene loaded");
            }
            info_timer = 3.0f;
        }

        /* Update info timer */
        if (info_timer > 0.0f) {
            info_timer -= agentite_get_delta_time(engine);
            if (info_timer <= 0.0f) {
                info_message[0] = '\0';
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(engine);
        }

        /* Progress ECS */
        agentite_ecs_progress(ecs_world, agentite_get_delta_time(engine));

        /* Build sprite batch - render all scene entities */
        agentite_sprite_begin(sprites, NULL);

        if (current_scene && agentite_scene_is_instantiated(current_scene)) {
            ecs_entity_t entities[256];
            size_t count = agentite_scene_get_entities(current_scene, entities, 256);

            for (size_t i = 0; i < count; i++) {
                ecs_entity_t e = entities[i];
                if (!ecs_is_alive(world, e)) continue;

                const C_Position *pos = ecs_get(world, e, C_Position);
                const C_SpriteRef *sref = ecs_get(world, e, C_SpriteRef);

                if (pos && sref) {
                    Agentite_Sprite *sprite = NULL;
                    switch (sref->texture_id) {
                        case 0: sprite = &sprite_player; break;
                        case 1: sprite = &sprite_enemy; break;
                        case 2: sprite = &sprite_item; break;
                        case 3: sprite = &sprite_platform; break;
                    }

                    if (sprite) {
                        agentite_sprite_draw_scaled(sprites, sprite,
                            pos->x, pos->y, sref->scale, sref->scale);
                    }
                } else if (pos) {
                    /* Entity without sprite ref - draw as platform */
                    agentite_sprite_draw(sprites, &sprite_platform, pos->x, pos->y);
                }
            }
        }

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            agentite_sprite_upload(sprites, cmd);

            /* Build text batch for HUD */
            if (font) {
                agentite_text_begin(text);

                const char *scene_name = current_scene ?
                    agentite_scene_get_name(current_scene) : "None";
                size_t ent_count = current_scene ?
                    agentite_scene_get_entity_count(current_scene) : 0;

                char buf[128];
                snprintf(buf, sizeof(buf), "Scene: %s  |  Entities: %zu",
                         scene_name, ent_count);
                agentite_text_draw_colored(text, font, buf, 10, 10, 1.0f, 1.0f, 1.0f, 1.0f);

                agentite_text_draw_colored(text, font,
                    "1/2: Load Scene | F: Find Player | I: Scene Info | ESC: Quit",
                    10, 30, 0.7f, 0.7f, 0.7f, 1.0f);

                /* Show info message if active */
                if (info_message[0] != '\0') {
                    agentite_text_draw_colored(text, font, info_message,
                        10, 60, 0.3f, 1.0f, 0.3f, 1.0f);
                }

                agentite_text_end(text);
                agentite_text_upload(text, cmd);
            }

            if (agentite_begin_render_pass(engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                agentite_sprite_render(sprites, cmd, pass);
                if (font) {
                    agentite_text_render(text, cmd, pass);
                }
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_texture_destroy(sprites, tex_player);
    agentite_texture_destroy(sprites, tex_enemy);
    agentite_texture_destroy(sprites, tex_item);
    agentite_texture_destroy(sprites, tex_platform);

    if (font) agentite_font_destroy(text, font);
    agentite_text_shutdown(text);
    agentite_scene_manager_destroy(scenes);
    agentite_prefab_registry_destroy(prefabs);
    agentite_reflect_destroy(reflect);
    agentite_ecs_shutdown(ecs_world);
    agentite_input_shutdown(input);
    agentite_camera_destroy(camera);
    agentite_sprite_shutdown(sprites);
    agentite_shutdown(engine);

    printf("Scene example finished.\n");
    return 0;
}
