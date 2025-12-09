#include "carbon/carbon.h"
#include "carbon/ecs.h"
#include <stdlib.h>
#include <stdio.h>

struct Carbon_World {
    ecs_world_t *world;
};

// Component IDs (populated during registration)
ECS_COMPONENT_DECLARE(C_Position);
ECS_COMPONENT_DECLARE(C_Velocity);
ECS_COMPONENT_DECLARE(C_Size);
ECS_COMPONENT_DECLARE(C_Color);
ECS_COMPONENT_DECLARE(C_Name);
ECS_COMPONENT_DECLARE(C_Active);
ECS_COMPONENT_DECLARE(C_Health);
ECS_COMPONENT_DECLARE(C_RenderLayer);

Carbon_World *carbon_ecs_init(void) {
    Carbon_World *cworld = CARBON_ALLOC(Carbon_World);
    if (!cworld) {
        return NULL;
    }

    cworld->world = ecs_init();
    if (!cworld->world) {
        free(cworld);
        return NULL;
    }

    // Register built-in components
    carbon_ecs_register_components(cworld);

    ecs_log(1, "Carbon ECS initialized with Flecs v%d.%d.%d",
            FLECS_VERSION_MAJOR, FLECS_VERSION_MINOR, FLECS_VERSION_PATCH);

    return cworld;
}

void carbon_ecs_shutdown(Carbon_World *world) {
    if (!world) return;

    if (world->world) {
        /* Flush any pending deferred operations before shutdown */
        while (ecs_is_deferred(world->world)) {
            ecs_defer_end(world->world);
        }

        ecs_fini(world->world);
    }

    free(world);
    ecs_log(1, "Carbon ECS shutdown complete");
}

ecs_world_t *carbon_ecs_get_world(Carbon_World *world) {
    return world ? world->world : NULL;
}

bool carbon_ecs_progress(Carbon_World *world, float delta_time) {
    if (!world || !world->world) return false;
    return ecs_progress(world->world, delta_time);
}

ecs_entity_t carbon_ecs_entity_new(Carbon_World *world) {
    if (!world || !world->world) return 0;
    return ecs_new(world->world);
}

ecs_entity_t carbon_ecs_entity_new_named(Carbon_World *world, const char *name) {
    if (!world || !world->world) return 0;
    ecs_entity_desc_t desc = {};
    desc.name = name;
    return ecs_entity_init(world->world, &desc);
}

void carbon_ecs_entity_delete(Carbon_World *world, ecs_entity_t entity) {
    if (!world || !world->world) return;
    ecs_delete(world->world, entity);
}

bool carbon_ecs_entity_is_alive(Carbon_World *world, ecs_entity_t entity) {
    if (!world || !world->world) return false;
    return ecs_is_alive(world->world, entity);
}

void carbon_ecs_register_components(Carbon_World *world) {
    if (!world || !world->world) return;

    ecs_world_t *w = world->world;

    ECS_COMPONENT_DEFINE(w, C_Position);
    ECS_COMPONENT_DEFINE(w, C_Velocity);
    ECS_COMPONENT_DEFINE(w, C_Size);
    ECS_COMPONENT_DEFINE(w, C_Color);
    ECS_COMPONENT_DEFINE(w, C_Name);
    ECS_COMPONENT_DEFINE(w, C_Active);
    ECS_COMPONENT_DEFINE(w, C_Health);
    ECS_COMPONENT_DEFINE(w, C_RenderLayer);
}
