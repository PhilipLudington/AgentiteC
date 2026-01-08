/*
 * Agentite ECS Tests
 *
 * Tests for the ECS wrapper around Flecs, including world lifecycle,
 * entity operations, and component management.
 */

#include "catch_amalgamated.hpp"
#include "agentite/ecs.h"

/* ============================================================================
 * World Lifecycle Tests
 * ============================================================================ */

TEST_CASE("ECS world lifecycle", "[ecs][lifecycle]") {
    SECTION("Create and destroy world") {
        Agentite_World *world = agentite_ecs_init();
        REQUIRE(world != nullptr);
        agentite_ecs_shutdown(world);
    }

    SECTION("Shutdown NULL world is safe") {
        agentite_ecs_shutdown(nullptr);
        // Should not crash
    }

    SECTION("Get underlying Flecs world") {
        Agentite_World *world = agentite_ecs_init();
        REQUIRE(world != nullptr);

        ecs_world_t *ecs = agentite_ecs_get_world(world);
        REQUIRE(ecs != nullptr);

        agentite_ecs_shutdown(world);
    }

    SECTION("Get world from NULL returns NULL") {
        ecs_world_t *ecs = agentite_ecs_get_world(nullptr);
        REQUIRE(ecs == nullptr);
    }
}

/* ============================================================================
 * Component Registration Tests
 * ============================================================================ */

TEST_CASE("ECS component registration", "[ecs][components]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);

    SECTION("Register common components") {
        agentite_ecs_register_components(world);
        // Should not crash, components should be available
    }

    SECTION("Register components is idempotent") {
        agentite_ecs_register_components(world);
        agentite_ecs_register_components(world);
        // Should not crash when called multiple times
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Entity Creation Tests
 * ============================================================================ */

TEST_CASE("ECS entity creation", "[ecs][entity]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);
    agentite_ecs_register_components(world);

    SECTION("Create entity") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);
        REQUIRE(entity != 0);
        REQUIRE(agentite_ecs_entity_is_alive(world, entity));
    }

    SECTION("Create multiple entities") {
        ecs_entity_t e1 = agentite_ecs_entity_new(world);
        ecs_entity_t e2 = agentite_ecs_entity_new(world);
        ecs_entity_t e3 = agentite_ecs_entity_new(world);

        REQUIRE(e1 != 0);
        REQUIRE(e2 != 0);
        REQUIRE(e3 != 0);

        // All entities should be unique
        REQUIRE(e1 != e2);
        REQUIRE(e2 != e3);
        REQUIRE(e1 != e3);
    }

    SECTION("Create named entity") {
        ecs_entity_t entity = agentite_ecs_entity_new_named(world, "Player");
        REQUIRE(entity != 0);
        REQUIRE(agentite_ecs_entity_is_alive(world, entity));
    }

    SECTION("Create entity with NULL name") {
        // Should return 0 or a valid entity depending on implementation
        // At minimum should not crash
        (void)agentite_ecs_entity_new_named(world, nullptr);
    }

    SECTION("Create entity from NULL world returns 0") {
        ecs_entity_t entity = agentite_ecs_entity_new(nullptr);
        REQUIRE(entity == 0);
    }

    SECTION("Create named entity from NULL world returns 0") {
        ecs_entity_t entity = agentite_ecs_entity_new_named(nullptr, "Test");
        REQUIRE(entity == 0);
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Entity Deletion Tests
 * ============================================================================ */

TEST_CASE("ECS entity deletion", "[ecs][entity]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);
    agentite_ecs_register_components(world);

    SECTION("Delete entity") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);
        REQUIRE(agentite_ecs_entity_is_alive(world, entity));

        agentite_ecs_entity_delete(world, entity);

        // Process deferred operations
        agentite_ecs_progress(world, 0.0f);

        REQUIRE_FALSE(agentite_ecs_entity_is_alive(world, entity));
    }

    SECTION("Delete non-existent entity is safe") {
        // Note: Flecs asserts on entity 0, so we only test non-zero entities
        agentite_ecs_entity_delete(world, 999999);
        // Should not crash
    }

    SECTION("Delete from NULL world is safe") {
        agentite_ecs_entity_delete(nullptr, 1);
        // Should not crash
    }

    SECTION("Double delete is safe") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);
        agentite_ecs_entity_delete(world, entity);
        agentite_ecs_progress(world, 0.0f);
        agentite_ecs_entity_delete(world, entity);
        // Should not crash
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Entity Alive Check Tests
 * ============================================================================ */

TEST_CASE("ECS entity is_alive", "[ecs][entity]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);

    SECTION("New entity is alive") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);
        REQUIRE(agentite_ecs_entity_is_alive(world, entity));
    }

    SECTION("Non-existent entity is not alive") {
        // Note: Flecs asserts on entity 0, so we test with a high invalid entity ID
        REQUIRE_FALSE(agentite_ecs_entity_is_alive(world, 999999999));
    }

    SECTION("NULL world returns false") {
        bool alive = agentite_ecs_entity_is_alive(nullptr, 1);
        REQUIRE_FALSE(alive);
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Component Operations Tests (Using direct Flecs API)
 * ============================================================================ */

TEST_CASE("ECS component operations", "[ecs][components]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);
    agentite_ecs_register_components(world);
    ecs_world_t *ecs = agentite_ecs_get_world(world);

    SECTION("Set and get C_Position component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Position pos = {100.0f, 200.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Position), sizeof(C_Position), &pos);

        const C_Position *got = AGENTITE_ECS_GET(world, entity, C_Position);
        REQUIRE(got != nullptr);
        REQUIRE(got->x == 100.0f);
        REQUIRE(got->y == 200.0f);
    }

    SECTION("Set and get C_Velocity component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Velocity vel = {5.0f, -3.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Velocity), sizeof(C_Velocity), &vel);

        const C_Velocity *got = AGENTITE_ECS_GET(world, entity, C_Velocity);
        REQUIRE(got != nullptr);
        REQUIRE(got->vx == 5.0f);
        REQUIRE(got->vy == -3.0f);
    }

    SECTION("Set and get C_Health component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Health health = {75, 100};
        ecs_set_id(ecs, entity, ecs_id(C_Health), sizeof(C_Health), &health);

        const C_Health *got = AGENTITE_ECS_GET(world, entity, C_Health);
        REQUIRE(got != nullptr);
        REQUIRE(got->health == 75);
        REQUIRE(got->max_health == 100);
    }

    SECTION("Set and get C_Color component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Color color = {1.0f, 0.5f, 0.25f, 1.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Color), sizeof(C_Color), &color);

        const C_Color *got = AGENTITE_ECS_GET(world, entity, C_Color);
        REQUIRE(got != nullptr);
        REQUIRE(got->r == 1.0f);
        REQUIRE(got->g == 0.5f);
        REQUIRE(got->b == 0.25f);
        REQUIRE(got->a == 1.0f);
    }

    SECTION("Set and get C_Size component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Size size = {64.0f, 32.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Size), sizeof(C_Size), &size);

        const C_Size *got = AGENTITE_ECS_GET(world, entity, C_Size);
        REQUIRE(got != nullptr);
        REQUIRE(got->width == 64.0f);
        REQUIRE(got->height == 32.0f);
    }

    SECTION("Set and get C_Active component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Active active = {true};
        ecs_set_id(ecs, entity, ecs_id(C_Active), sizeof(C_Active), &active);

        const C_Active *got = AGENTITE_ECS_GET(world, entity, C_Active);
        REQUIRE(got != nullptr);
        REQUIRE(got->active == true);
    }

    SECTION("Set and get C_RenderLayer component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_RenderLayer layer = {5};
        ecs_set_id(ecs, entity, ecs_id(C_RenderLayer), sizeof(C_RenderLayer), &layer);

        const C_RenderLayer *got = AGENTITE_ECS_GET(world, entity, C_RenderLayer);
        REQUIRE(got != nullptr);
        REQUIRE(got->layer == 5);
    }

    SECTION("Get non-existent component returns NULL") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);
        // Don't set any component
        const C_Position *pos = AGENTITE_ECS_GET(world, entity, C_Position);
        REQUIRE(pos == nullptr);
    }

    SECTION("Update component value") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Position pos1 = {0.0f, 0.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Position), sizeof(C_Position), &pos1);

        C_Position pos2 = {100.0f, 200.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Position), sizeof(C_Position), &pos2);

        const C_Position *got = AGENTITE_ECS_GET(world, entity, C_Position);
        REQUIRE(got != nullptr);
        REQUIRE(got->x == 100.0f);
        REQUIRE(got->y == 200.0f);
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Component Add/Remove Tests
 * ============================================================================ */

TEST_CASE("ECS component add and remove", "[ecs][components]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);
    agentite_ecs_register_components(world);
    ecs_world_t *ecs = agentite_ecs_get_world(world);

    SECTION("Add component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        REQUIRE_FALSE(AGENTITE_ECS_HAS(world, entity, C_Position));

        AGENTITE_ECS_ADD(world, entity, C_Position);

        REQUIRE(AGENTITE_ECS_HAS(world, entity, C_Position));
    }

    SECTION("Remove component") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Position pos = {10.0f, 20.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Position), sizeof(C_Position), &pos);
        REQUIRE(AGENTITE_ECS_HAS(world, entity, C_Position));

        AGENTITE_ECS_REMOVE(world, entity, C_Position);

        REQUIRE_FALSE(AGENTITE_ECS_HAS(world, entity, C_Position));
    }

    SECTION("Has component check") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        REQUIRE_FALSE(AGENTITE_ECS_HAS(world, entity, C_Position));
        REQUIRE_FALSE(AGENTITE_ECS_HAS(world, entity, C_Velocity));

        C_Position pos = {0.0f, 0.0f};
        ecs_set_id(ecs, entity, ecs_id(C_Position), sizeof(C_Position), &pos);

        REQUIRE(AGENTITE_ECS_HAS(world, entity, C_Position));
        REQUIRE_FALSE(AGENTITE_ECS_HAS(world, entity, C_Velocity));
    }

    SECTION("Multiple components on entity") {
        ecs_entity_t entity = agentite_ecs_entity_new(world);

        C_Position pos = {10.0f, 20.0f};
        C_Velocity vel = {1.0f, 2.0f};
        C_Health health = {100, 100};

        ecs_set_id(ecs, entity, ecs_id(C_Position), sizeof(C_Position), &pos);
        ecs_set_id(ecs, entity, ecs_id(C_Velocity), sizeof(C_Velocity), &vel);
        ecs_set_id(ecs, entity, ecs_id(C_Health), sizeof(C_Health), &health);

        REQUIRE(AGENTITE_ECS_HAS(world, entity, C_Position));
        REQUIRE(AGENTITE_ECS_HAS(world, entity, C_Velocity));
        REQUIRE(AGENTITE_ECS_HAS(world, entity, C_Health));

        const C_Position *got_pos = AGENTITE_ECS_GET(world, entity, C_Position);
        const C_Velocity *got_vel = AGENTITE_ECS_GET(world, entity, C_Velocity);
        const C_Health *got_health = AGENTITE_ECS_GET(world, entity, C_Health);

        REQUIRE(got_pos->x == 10.0f);
        REQUIRE(got_vel->vx == 1.0f);
        REQUIRE(got_health->health == 100);
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * World Progress Tests
 * ============================================================================ */

TEST_CASE("ECS world progress", "[ecs][progress]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);

    SECTION("Progress with zero delta time") {
        bool running = agentite_ecs_progress(world, 0.0f);
        REQUIRE(running == true);
    }

    SECTION("Progress with positive delta time") {
        bool running = agentite_ecs_progress(world, 0.016f); // ~60fps
        REQUIRE(running == true);
    }

    SECTION("Progress NULL world returns false") {
        bool running = agentite_ecs_progress(nullptr, 0.016f);
        REQUIRE_FALSE(running);
    }

    SECTION("Multiple progress calls") {
        for (int i = 0; i < 100; i++) {
            bool running = agentite_ecs_progress(world, 0.016f);
            REQUIRE(running == true);
        }
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Component Struct Tests
 * ============================================================================ */

TEST_CASE("Common component struct sizes", "[ecs][struct]") {
    SECTION("C_Position struct layout") {
        C_Position pos = {0.0f, 0.0f};
        pos.x = 100.0f;
        pos.y = 200.0f;
        REQUIRE(pos.x == 100.0f);
        REQUIRE(pos.y == 200.0f);
        REQUIRE(sizeof(C_Position) == 2 * sizeof(float));
    }

    SECTION("C_Velocity struct layout") {
        C_Velocity vel = {0.0f, 0.0f};
        vel.vx = 1.0f;
        vel.vy = -1.0f;
        REQUIRE(vel.vx == 1.0f);
        REQUIRE(vel.vy == -1.0f);
        REQUIRE(sizeof(C_Velocity) == 2 * sizeof(float));
    }

    SECTION("C_Size struct layout") {
        C_Size size = {0.0f, 0.0f};
        size.width = 64.0f;
        size.height = 32.0f;
        REQUIRE(size.width == 64.0f);
        REQUIRE(size.height == 32.0f);
        REQUIRE(sizeof(C_Size) == 2 * sizeof(float));
    }

    SECTION("C_Color struct layout") {
        C_Color color = {0.0f, 0.0f, 0.0f, 0.0f};
        color.r = 1.0f;
        color.g = 0.5f;
        color.b = 0.25f;
        color.a = 1.0f;
        REQUIRE(sizeof(C_Color) == 4 * sizeof(float));
    }

    SECTION("C_Health struct layout") {
        C_Health health = {0, 0};
        health.health = 50;
        health.max_health = 100;
        REQUIRE(health.health == 50);
        REQUIRE(health.max_health == 100);
        REQUIRE(sizeof(C_Health) == 2 * sizeof(int));
    }

    SECTION("C_Active struct layout") {
        C_Active active = {false};
        active.active = true;
        REQUIRE(active.active == true);
        REQUIRE(sizeof(C_Active) >= sizeof(bool));
    }

    SECTION("C_RenderLayer struct layout") {
        C_RenderLayer layer = {0};
        layer.layer = 10;
        REQUIRE(layer.layer == 10);
        REQUIRE(sizeof(C_RenderLayer) == sizeof(int));
    }
}
