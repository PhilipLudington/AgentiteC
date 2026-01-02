/**
 * Agentite Engine - Prefab Example
 *
 * Demonstrates loading entity prefabs from files and spawning them on click.
 * Click anywhere to spawn the currently selected prefab type.
 * Press 1/2/3 to select different prefab types.
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/text.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include "agentite/ecs.h"
#include "agentite/ecs_reflect.h"
#include "agentite/prefab.h"
#include "agentite/transform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Custom Game Components
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

ECS_COMPONENT_DECLARE(C_EnemyType);
ECS_COMPONENT_DECLARE(C_Item);
ECS_COMPONENT_DECLARE(C_SpriteRef);

/* ============================================================================
 * Spawned entity tracking - store position directly to avoid ECS query issues
 * ============================================================================ */

typedef struct SpawnedEntity {
    ecs_entity_t entity;
    float x, y;
    int type;  /* 0=enemy, 1=item, 2=player */
} SpawnedEntity;

#define MAX_ENTITIES 256

/* ============================================================================
 * Helper: Create colored texture
 * ============================================================================ */

static Agentite_Texture *create_colored_texture(Agentite_SpriteRenderer *sr,
                                                  int size,
                                                  uint8_t r, uint8_t g, uint8_t b) {
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            bool is_border = (x < 2 || x >= size - 2 || y < 2 || y >= size - 2);
            if (is_border) {
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
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Agentite Prefab Example ===\n");
    printf("Controls:\n");
    printf("  Click      - Spawn entity at mouse position\n");
    printf("  1/2/3      - Select Enemy(red) / Item(gold) / Player(blue)\n");
    printf("  C          - Clear all entities\n");
    printf("  ESC        - Quit\n\n");

    /* Initialize engine */
    Agentite_Config config = {
        .window_title = "Agentite - Prefab Example",
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

    /* Initialize camera */
    Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_sprite_set_camera(sprites, camera);
    agentite_camera_set_position(camera, 640.0f, 360.0f);

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

    /* Initialize prefab registry */
    Agentite_PrefabRegistry *prefabs = agentite_prefab_registry_create();

    /* Load prefabs */
    Agentite_Prefab *enemy_prefab = agentite_prefab_load(
        prefabs, "examples/prefab/prefabs/enemy.prefab", reflect);
    Agentite_Prefab *item_prefab = agentite_prefab_load(
        prefabs, "examples/prefab/prefabs/item.prefab", reflect);
    Agentite_Prefab *player_prefab = agentite_prefab_load(
        prefabs, "examples/prefab/prefabs/player.prefab", reflect);

    printf("Loaded %zu prefabs\n", agentite_prefab_registry_count(prefabs));

    /* Create textures */
    Agentite_Texture *tex_enemy = create_colored_texture(sprites, 32, 255, 80, 80);
    Agentite_Texture *tex_item = create_colored_texture(sprites, 24, 255, 215, 0);
    Agentite_Texture *tex_player = create_colored_texture(sprites, 40, 80, 150, 255);

    Agentite_Sprite sprite_enemy = agentite_sprite_from_texture(tex_enemy);
    Agentite_Sprite sprite_item = agentite_sprite_from_texture(tex_item);
    Agentite_Sprite sprite_player = agentite_sprite_from_texture(tex_player);

    /* Track spawned entities with their positions */
    SpawnedEntity spawned[MAX_ENTITIES];
    int entity_count = 0;

    /* Currently selected prefab */
    int selected_prefab = 0;
    const char *prefab_short[] = {"Enemy", "Item", "Player"};

    /* Track mouse state to only spawn once per click */
    bool was_mouse_down = false;

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

        /* Update camera */
        agentite_camera_update(camera);

        /* Handle prefab selection */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_1)) {
            selected_prefab = 0;
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_2)) {
            selected_prefab = 1;
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_3)) {
            selected_prefab = 2;
        }

        /* Check mouse state - only spawn on click DOWN, not while held */
        bool mouse_down = agentite_input_mouse_button(input, 0);
        bool just_clicked = mouse_down && !was_mouse_down;
        was_mouse_down = mouse_down;

        /* Spawn on click */
        if (just_clicked && entity_count < MAX_ENTITIES) {
            float mx, my;
            agentite_input_get_mouse_position(input, &mx, &my);

            /* Convert screen to world coordinates */
            float world_x, world_y;
            agentite_camera_screen_to_world(camera, mx, my, &world_x, &world_y);

            /* Select prefab */
            Agentite_Prefab *prefab = NULL;
            switch (selected_prefab) {
                case 0: prefab = enemy_prefab; break;
                case 1: prefab = item_prefab; break;
                case 2: prefab = player_prefab; break;
            }

            if (prefab) {
                ecs_entity_t e = agentite_prefab_spawn_at(
                    prefab, world, reflect, world_x, world_y);

                if (e != 0) {
                    /* Store entity with its position */
                    spawned[entity_count].entity = e;
                    spawned[entity_count].x = world_x;
                    spawned[entity_count].y = world_y;
                    spawned[entity_count].type = selected_prefab;
                    entity_count++;

                    printf("Spawned %s at (%.0f, %.0f) - total: %d\n",
                           prefab_short[selected_prefab], world_x, world_y, entity_count);
                }
            }
        }

        /* Clear entities with C key */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_C)) {
            for (int i = 0; i < entity_count; i++) {
                if (ecs_is_alive(world, spawned[i].entity)) {
                    ecs_delete(world, spawned[i].entity);
                }
            }
            entity_count = 0;
            printf("Cleared all entities\n");
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(engine);
        }

        /* Progress ECS */
        agentite_ecs_progress(ecs_world, agentite_get_delta_time(engine));

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Build sprite batch */
            agentite_sprite_begin(sprites, cmd);
            for (int i = 0; i < entity_count; i++) {
                Agentite_Sprite *sprite = NULL;
                switch (spawned[i].type) {
                    case 0: sprite = &sprite_enemy; break;
                    case 1: sprite = &sprite_item; break;
                    case 2: sprite = &sprite_player; break;
                }
                if (sprite) {
                    agentite_sprite_draw(sprites, sprite, spawned[i].x, spawned[i].y);
                }
            }
            agentite_sprite_upload(sprites, cmd);

            /* Build text batch for HUD */
            if (font) {
                agentite_text_begin(text);

                char buf[128];
                snprintf(buf, sizeof(buf), "Selected: %s  |  Entities: %d",
                         prefab_short[selected_prefab], entity_count);
                agentite_text_draw_colored(text, font, buf, 10, 10, 1.0f, 1.0f, 1.0f, 1.0f);

                agentite_text_draw_colored(text, font,
                    "Click: Spawn | 1/2/3: Select Enemy/Item/Player | C: Clear | ESC: Quit",
                    10, 30, 0.7f, 0.7f, 0.7f, 1.0f);

                agentite_text_end(text);
                agentite_text_upload(text, cmd);
            }

            /* Render pass */
            if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                agentite_sprite_render(sprites, cmd, agentite_get_render_pass(engine));
                if (font) {
                    agentite_text_render(text, cmd, agentite_get_render_pass(engine));
                }
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_texture_destroy(sprites, tex_enemy);
    agentite_texture_destroy(sprites, tex_item);
    agentite_texture_destroy(sprites, tex_player);

    if (font) agentite_font_destroy(text, font);
    agentite_text_shutdown(text);
    agentite_prefab_registry_destroy(prefabs);
    agentite_reflect_destroy(reflect);
    agentite_ecs_shutdown(ecs_world);
    agentite_input_shutdown(input);
    agentite_camera_destroy(camera);
    agentite_sprite_shutdown(sprites);
    agentite_shutdown(engine);

    printf("Prefab example finished.\n");
    return 0;
}
