/*
 * Agentite Engine - Prefab System Tests
 *
 * Tests for prefab loading, parsing, and spawning.
 */

#include "catch_amalgamated.hpp"
#include "agentite/prefab.h"
#include "agentite/ecs.h"
#include "agentite/ecs_reflect.h"
#include <cstring>

/* ============================================================================
 * Test Component Types
 * ============================================================================ */

typedef struct TestPosition {
    float x;
    float y;
} TestPosition;

typedef struct TestHealth {
    int current;
    int max;
} TestHealth;

typedef struct TestSprite {
    const char *texture_path;
} TestSprite;

typedef struct TestStats {
    int strength;
    int defense;
    float speed;
} TestStats;

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class PrefabTestFixture {
public:
    Agentite_ReflectRegistry *reflect = nullptr;
    Agentite_PrefabRegistry *prefabs = nullptr;
    Agentite_World *world = nullptr;

    /* Component IDs */
    ecs_entity_t c_position = 0;
    ecs_entity_t c_health = 0;
    ecs_entity_t c_sprite = 0;
    ecs_entity_t c_stats = 0;

    PrefabTestFixture() {
        reflect = agentite_reflect_create();
        prefabs = agentite_prefab_registry_create();
        world = agentite_ecs_init();

        if (world) {
            ecs_world_t *ecs = agentite_ecs_get_world(world);

            /* Register test components */
            ecs_entity_desc_t pos_edesc = {}; pos_edesc.name = "TestPosition";
            ecs_component_desc_t pos_desc = {};
            pos_desc.entity = ecs_entity_init(ecs, &pos_edesc);
            pos_desc.type.size = sizeof(TestPosition);
            pos_desc.type.alignment = alignof(TestPosition);
            c_position = ecs_component_init(ecs, &pos_desc);

            ecs_entity_desc_t health_edesc = {}; health_edesc.name = "TestHealth";
            ecs_component_desc_t health_desc = {};
            health_desc.entity = ecs_entity_init(ecs, &health_edesc);
            health_desc.type.size = sizeof(TestHealth);
            health_desc.type.alignment = alignof(TestHealth);
            c_health = ecs_component_init(ecs, &health_desc);

            ecs_entity_desc_t sprite_edesc = {}; sprite_edesc.name = "TestSprite";
            ecs_component_desc_t sprite_desc = {};
            sprite_desc.entity = ecs_entity_init(ecs, &sprite_edesc);
            sprite_desc.type.size = sizeof(TestSprite);
            sprite_desc.type.alignment = alignof(TestSprite);
            c_sprite = ecs_component_init(ecs, &sprite_desc);

            ecs_entity_desc_t stats_edesc = {}; stats_edesc.name = "TestStats";
            ecs_component_desc_t stats_desc = {};
            stats_desc.entity = ecs_entity_init(ecs, &stats_edesc);
            stats_desc.type.size = sizeof(TestStats);
            stats_desc.type.alignment = alignof(TestStats);
            c_stats = ecs_component_init(ecs, &stats_desc);

            /* Register with reflection */
            Agentite_FieldDesc pos_fields[] = {
                { "x", AGENTITE_FIELD_FLOAT, offsetof(TestPosition, x), sizeof(float) },
                { "y", AGENTITE_FIELD_FLOAT, offsetof(TestPosition, y), sizeof(float) }
            };
            agentite_reflect_register(reflect, c_position, "TestPosition",
                sizeof(TestPosition), pos_fields, 2);

            Agentite_FieldDesc health_fields[] = {
                { "current", AGENTITE_FIELD_INT, offsetof(TestHealth, current), sizeof(int) },
                { "max", AGENTITE_FIELD_INT, offsetof(TestHealth, max), sizeof(int) }
            };
            agentite_reflect_register(reflect, c_health, "TestHealth",
                sizeof(TestHealth), health_fields, 2);

            Agentite_FieldDesc sprite_fields[] = {
                { "texture_path", AGENTITE_FIELD_STRING, offsetof(TestSprite, texture_path), sizeof(const char*) }
            };
            agentite_reflect_register(reflect, c_sprite, "TestSprite",
                sizeof(TestSprite), sprite_fields, 1);

            Agentite_FieldDesc stats_fields[] = {
                { "strength", AGENTITE_FIELD_INT, offsetof(TestStats, strength), sizeof(int) },
                { "defense", AGENTITE_FIELD_INT, offsetof(TestStats, defense), sizeof(int) },
                { "speed", AGENTITE_FIELD_FLOAT, offsetof(TestStats, speed), sizeof(float) }
            };
            agentite_reflect_register(reflect, c_stats, "TestStats",
                sizeof(TestStats), stats_fields, 3);

            /* Also register C_Position for position handling */
            agentite_ecs_register_components(world);
            Agentite_FieldDesc cpos_fields[] = {
                { "x", AGENTITE_FIELD_FLOAT, offsetof(C_Position, x), sizeof(float) },
                { "y", AGENTITE_FIELD_FLOAT, offsetof(C_Position, y), sizeof(float) }
            };
            agentite_reflect_register(reflect, ecs_id(C_Position), "C_Position",
                sizeof(C_Position), cpos_fields, 2);
        }
    }

    ~PrefabTestFixture() {
        agentite_ecs_shutdown(world);
        agentite_prefab_registry_destroy(prefabs);
        agentite_reflect_destroy(reflect);
    }
};

/* ============================================================================
 * Lexer/Parser Tests
 * ============================================================================ */

TEST_CASE("Prefab parsing - simple entity", "[prefab][parse]") {
    const char *source = R"(
        Entity Player @(100, 200) {
            TestHealth: { current: 50, max: 100 }
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
    REQUIRE(prefab != nullptr);

    SECTION("Entity name") {
        REQUIRE(prefab->name != nullptr);
        REQUIRE(strcmp(prefab->name, "Player") == 0);
    }

    SECTION("Position") {
        REQUIRE(prefab->position[0] == 100.0f);
        REQUIRE(prefab->position[1] == 200.0f);
    }

    SECTION("Component count") {
        REQUIRE(prefab->component_count == 1);
    }

    SECTION("Component configuration") {
        REQUIRE(strcmp(prefab->components[0].component_name, "TestHealth") == 0);
        REQUIRE(prefab->components[0].field_count == 2);
    }

    agentite_prefab_destroy(prefab);
}

TEST_CASE("Prefab parsing - simple value syntax", "[prefab][parse]") {
    const char *source = R"(
        Entity {
            Health: 100
            Speed: 5.5
            Active: true
            Name: "Player One"
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
    REQUIRE(prefab != nullptr);
    REQUIRE(prefab->component_count == 4);

    /* Health: 100 should create field "value" with int 100 */
    REQUIRE(strcmp(prefab->components[0].component_name, "Health") == 0);
    REQUIRE(prefab->components[0].field_count == 1);
    REQUIRE(strcmp(prefab->components[0].fields[0].field_name, "value") == 0);
    REQUIRE(prefab->components[0].fields[0].value.type == AGENTITE_PROP_INT);
    REQUIRE(prefab->components[0].fields[0].value.int_val == 100);

    /* Speed: 5.5 should create float value */
    REQUIRE(prefab->components[1].fields[0].value.type == AGENTITE_PROP_FLOAT);
    REQUIRE(prefab->components[1].fields[0].value.float_val == Catch::Approx(5.5));

    /* Active: true should create bool value */
    REQUIRE(prefab->components[2].fields[0].value.type == AGENTITE_PROP_BOOL);
    REQUIRE(prefab->components[2].fields[0].value.bool_val == true);

    /* Name: "Player One" should create string value */
    REQUIRE(prefab->components[3].fields[0].value.type == AGENTITE_PROP_STRING);
    REQUIRE(strcmp(prefab->components[3].fields[0].value.string_val, "Player One") == 0);

    agentite_prefab_destroy(prefab);
}

TEST_CASE("Prefab parsing - vector values", "[prefab][parse]") {
    const char *source = R"(
        Entity {
            Position: (10, 20)
            Velocity: (1.5, -2.5, 0)
            Color: (1.0, 0.5, 0.2, 1.0)
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
    REQUIRE(prefab != nullptr);
    REQUIRE(prefab->component_count == 3);

    /* Vec2 */
    REQUIRE(prefab->components[0].fields[0].value.type == AGENTITE_PROP_VEC2);
    REQUIRE(prefab->components[0].fields[0].value.vec2_val[0] == Catch::Approx(10));
    REQUIRE(prefab->components[0].fields[0].value.vec2_val[1] == Catch::Approx(20));

    /* Vec3 */
    REQUIRE(prefab->components[1].fields[0].value.type == AGENTITE_PROP_VEC3);
    REQUIRE(prefab->components[1].fields[0].value.vec3_val[0] == Catch::Approx(1.5));
    REQUIRE(prefab->components[1].fields[0].value.vec3_val[1] == Catch::Approx(-2.5));
    REQUIRE(prefab->components[1].fields[0].value.vec3_val[2] == Catch::Approx(0));

    /* Vec4 */
    REQUIRE(prefab->components[2].fields[0].value.type == AGENTITE_PROP_VEC4);
    REQUIRE(prefab->components[2].fields[0].value.vec4_val[0] == Catch::Approx(1.0));
    REQUIRE(prefab->components[2].fields[0].value.vec4_val[3] == Catch::Approx(1.0));

    agentite_prefab_destroy(prefab);
}

TEST_CASE("Prefab parsing - nested entities", "[prefab][parse]") {
    const char *source = R"(
        Entity Player @(100, 100) {
            Health: 100

            Entity Weapon @(20, 0) {
                Damage: 25
            }

            Entity Shield @(-15, 0) {
                Defense: 10
            }
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
    REQUIRE(prefab != nullptr);
    REQUIRE(prefab->child_count == 2);

    /* First child */
    REQUIRE(strcmp(prefab->children[0]->name, "Weapon") == 0);
    REQUIRE(prefab->children[0]->position[0] == Catch::Approx(20));

    /* Second child */
    REQUIRE(strcmp(prefab->children[1]->name, "Shield") == 0);
    REQUIRE(prefab->children[1]->position[0] == Catch::Approx(-15));

    agentite_prefab_destroy(prefab);
}

TEST_CASE("Prefab parsing - comments", "[prefab][parse]") {
    const char *source = R"(
        // This is a comment
        Entity Player {
            // Component comment
            Health: 100  // Inline comment not supported yet but shouldn't break
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
    REQUIRE(prefab != nullptr);
    REQUIRE(prefab->component_count == 1);

    agentite_prefab_destroy(prefab);
}

TEST_CASE("Prefab parsing - error handling", "[prefab][parse]") {
    SECTION("Missing Entity keyword") {
        const char *source = "{ Health: 100 }";
        Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
        REQUIRE(prefab == nullptr);
    }

    SECTION("Unclosed brace") {
        const char *source = "Entity { Health: 100";
        Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
        REQUIRE(prefab == nullptr);
    }

    SECTION("Unterminated string") {
        const char *source = "Entity { Name: \"unclosed }";
        Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", nullptr);
        REQUIRE(prefab == nullptr);
    }
}

/* ============================================================================
 * Registry Tests
 * ============================================================================ */

TEST_CASE("Prefab registry lifecycle", "[prefab][registry]") {
    Agentite_PrefabRegistry *registry = agentite_prefab_registry_create();
    REQUIRE(registry != nullptr);
    REQUIRE(agentite_prefab_registry_count(registry) == 0);

    agentite_prefab_registry_destroy(registry);
}

TEST_CASE("Prefab registry - NULL is safe", "[prefab][registry]") {
    agentite_prefab_registry_destroy(nullptr);
    REQUIRE(agentite_prefab_registry_count(nullptr) == 0);
}

/* ============================================================================
 * Spawning Tests
 * ============================================================================ */

TEST_CASE_METHOD(PrefabTestFixture, "Prefab spawning - basic", "[prefab][spawn]") {
    const char *source = R"(
        Entity TestEntity @(50, 75) {
            TestHealth: { current: 80, max: 100 }
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", reflect);
    REQUIRE(prefab != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);
    ecs_entity_t entity = agentite_prefab_spawn_at(prefab, ecs, reflect, 100, 200);

    REQUIRE(entity != 0);

    /* Check if entity was created */
    REQUIRE(ecs_is_alive(ecs, entity));

    /* Check TestHealth component */
    const TestHealth *health = (const TestHealth *)ecs_get_id(ecs, entity, c_health);
    REQUIRE(health != nullptr);
    REQUIRE(health->current == 80);
    REQUIRE(health->max == 100);

    agentite_prefab_destroy(prefab);
}

TEST_CASE_METHOD(PrefabTestFixture, "Prefab spawning - position offset", "[prefab][spawn]") {
    const char *source = R"(
        Entity @(10, 20) {
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", reflect);
    REQUIRE(prefab != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);

    /* Spawn at (100, 200), prefab has offset (10, 20) */
    ecs_entity_t entity = agentite_prefab_spawn_at(prefab, ecs, reflect, 100, 200);
    REQUIRE(entity != 0);

    /* Position should be 100+10=110, 200+20=220 */
    const C_Position *pos = (const C_Position *)ecs_get_id(ecs, entity, ecs_id(C_Position));
    REQUIRE(pos != nullptr);
    REQUIRE(pos->x == Catch::Approx(110.0f));
    REQUIRE(pos->y == Catch::Approx(220.0f));

    agentite_prefab_destroy(prefab);
}

TEST_CASE_METHOD(PrefabTestFixture, "Prefab spawning - multiple components", "[prefab][spawn]") {
    const char *source = R"(
        Entity {
            TestHealth: { current: 50, max: 100 }
            TestStats: { strength: 15, defense: 8, speed: 1.5 }
        }
    )";

    Agentite_Prefab *prefab = agentite_prefab_load_string(source, 0, "test", reflect);
    REQUIRE(prefab != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);
    ecs_entity_t entity = agentite_prefab_spawn_at(prefab, ecs, reflect, 0, 0);
    REQUIRE(entity != 0);

    const TestHealth *health = (const TestHealth *)ecs_get_id(ecs, entity, c_health);
    REQUIRE(health != nullptr);
    REQUIRE(health->current == 50);

    const TestStats *stats = (const TestStats *)ecs_get_id(ecs, entity, c_stats);
    REQUIRE(stats != nullptr);
    REQUIRE(stats->strength == 15);
    REQUIRE(stats->defense == 8);
    REQUIRE(stats->speed == Catch::Approx(1.5f));

    agentite_prefab_destroy(prefab);
}
