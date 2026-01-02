/*
 * Agentite Engine - Scene System Tests
 *
 * Tests for scene loading, instantiation, transitions, and serialization.
 */

#include "catch_amalgamated.hpp"
#include "agentite/scene.h"
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

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class SceneTestFixture {
public:
    Agentite_ReflectRegistry *reflect = nullptr;
    Agentite_SceneManager *scenes = nullptr;
    Agentite_PrefabRegistry *prefabs = nullptr;
    Agentite_World *world = nullptr;

    /* Component IDs */
    ecs_entity_t c_position = 0;
    ecs_entity_t c_health = 0;
    ecs_entity_t c_sprite = 0;

    SceneTestFixture() {
        reflect = agentite_reflect_create();
        scenes = agentite_scene_manager_create();
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

    ~SceneTestFixture() {
        agentite_scene_manager_destroy(scenes);
        agentite_prefab_registry_destroy(prefabs);
        agentite_ecs_shutdown(world);
        agentite_reflect_destroy(reflect);
    }

    Agentite_SceneLoadContext make_context() {
        Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
        ctx.reflect = reflect;
        ctx.prefabs = prefabs;
        return ctx;
    }
};

/* ============================================================================
 * Scene Manager Tests
 * ============================================================================ */

TEST_CASE("Scene manager lifecycle", "[scene][manager]") {
    Agentite_SceneManager *manager = agentite_scene_manager_create();
    REQUIRE(manager != nullptr);
    REQUIRE(agentite_scene_manager_get_active(manager) == nullptr);

    agentite_scene_manager_destroy(manager);
}

TEST_CASE("Scene manager - NULL is safe", "[scene][manager]") {
    agentite_scene_manager_destroy(nullptr);
    REQUIRE(agentite_scene_manager_get_active(nullptr) == nullptr);
}

/* ============================================================================
 * Scene Parsing Tests
 * ============================================================================ */

TEST_CASE("Scene parsing - single entity", "[scene][parse]") {
    const char *source = R"(
        Entity Player @(100, 200) {
            TestHealth: { current: 50, max: 100 }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    SECTION("Root count") {
        REQUIRE(agentite_scene_get_root_count(scene) == 1);
    }

    SECTION("Scene name") {
        REQUIRE(agentite_scene_get_name(scene) != nullptr);
        REQUIRE(strcmp(agentite_scene_get_name(scene), "test") == 0);
    }

    SECTION("Scene state") {
        REQUIRE(agentite_scene_get_state(scene) == AGENTITE_SCENE_PARSED);
    }

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - multiple root entities", "[scene][parse]") {
    const char *source = R"(
        Entity Player @(100, 100) {
            TestHealth: { current: 100, max: 100 }
        }

        Entity Enemy @(300, 100) {
            TestHealth: { current: 50, max: 50 }
        }

        Entity Pickup @(200, 200) {
            TestSprite: "items/health.png"
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 3);

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - nested entities", "[scene][parse]") {
    const char *source = R"(
        Entity Player @(100, 100) {
            TestHealth: 100

            Entity Weapon @(20, 0) {
                TestSprite: "weapons/sword.png"
            }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 1);

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - error handling", "[scene][parse]") {
    SECTION("NULL source") {
        Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
        Agentite_Scene *scene = agentite_scene_load_string(nullptr, 0, "test", &ctx);
        REQUIRE(scene == nullptr);
    }

    SECTION("Empty source") {
        Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
        Agentite_Scene *scene = agentite_scene_load_string("", 0, "test", &ctx);
        REQUIRE(scene == nullptr);
    }

    SECTION("Invalid syntax") {
        const char *source = "{ not a valid scene }";
        Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
        Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
        REQUIRE(scene == nullptr);
    }
}

/* ============================================================================
 * New DSL Format Tests (AI-friendly format without Entity keyword)
 * ============================================================================ */

TEST_CASE("Scene parsing - new format without Entity keyword", "[scene][parse][newformat]") {
    const char *source = R"(
        Player @(100, 200) {
            TestHealth: { current: 50, max: 100 }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 1);

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - new format with hash comments", "[scene][parse][newformat]") {
    const char *source = R"(
        # This is a comment using hash
        # Another comment line
        Player @(100, 200) {
            # Component comment
            TestHealth: { current: 50, max: 100 }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 1);

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - new format multiple entities", "[scene][parse][newformat]") {
    const char *source = R"(
        # Player entity
        Player @(100, 100) {
            TestHealth: { current: 100, max: 100 }
        }

        # Enemy entity
        Enemy @(300, 100) {
            TestHealth: { current: 50, max: 50 }
        }

        # Pickup item
        Pickup @(200, 200) {
            TestSprite: "items/health.png"
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 3);

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - new format nested entities", "[scene][parse][newformat]") {
    const char *source = R"(
        Player @(100, 100) {
            TestHealth: 100

            Weapon @(20, 0) {
                TestSprite: "weapons/sword.png"
            }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 1);

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - backward compatibility with Entity keyword", "[scene][parse][compat]") {
    /* Old format should still work */
    const char *source = R"(
        Entity Player @(100, 200) {
            TestHealth: { current: 50, max: 100 }

            Entity Child @(10, 0) {
                TestSprite: "child.png"
            }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 1);

    agentite_scene_destroy(scene);
}

TEST_CASE("Scene parsing - mixed comment styles", "[scene][parse][newformat]") {
    const char *source = R"(
        // C-style comment
        # Hash comment
        Player @(100, 200) {
            // Another C-style
            TestHealth: 100
            # And a hash
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene) == 1);

    agentite_scene_destroy(scene);
}

/* ============================================================================
 * Scene Instantiation Tests
 * ============================================================================ */

TEST_CASE_METHOD(SceneTestFixture, "Scene instantiation - basic", "[scene][instantiate]") {
    const char *source = R"(
        Entity TestEntity @(50, 75) {
            TestHealth: { current: 80, max: 100 }
        }
    )";

    auto ctx = make_context();
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);
    bool result = agentite_scene_instantiate(scene, ecs, &ctx);
    REQUIRE(result == true);

    SECTION("State changes to loaded") {
        REQUIRE(agentite_scene_get_state(scene) == AGENTITE_SCENE_LOADED);
    }

    SECTION("Entity count") {
        REQUIRE(agentite_scene_get_entity_count(scene) == 1);
    }

    SECTION("Is instantiated") {
        REQUIRE(agentite_scene_is_instantiated(scene) == true);
    }

    SECTION("Entity has component") {
        ecs_entity_t entities[16];
        size_t count = agentite_scene_get_entities(scene, entities, 16);
        REQUIRE(count == 1);

        const TestHealth *health = (const TestHealth *)ecs_get_id(ecs, entities[0], c_health);
        REQUIRE(health != nullptr);
        REQUIRE(health->current == 80);
        REQUIRE(health->max == 100);
    }

    agentite_scene_destroy(scene);
}

TEST_CASE_METHOD(SceneTestFixture, "Scene instantiation - multiple entities", "[scene][instantiate]") {
    const char *source = R"(
        Entity Player @(100, 100) {
            TestHealth: { current: 100, max: 100 }
        }

        Entity Enemy @(300, 100) {
            TestHealth: { current: 50, max: 50 }
        }
    )";

    auto ctx = make_context();
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);
    bool result = agentite_scene_instantiate(scene, ecs, &ctx);
    REQUIRE(result == true);

    REQUIRE(agentite_scene_get_entity_count(scene) == 2);

    ecs_entity_t roots[16];
    size_t root_count = agentite_scene_get_root_entities(scene, roots, 16);
    REQUIRE(root_count == 2);

    agentite_scene_destroy(scene);
}

TEST_CASE_METHOD(SceneTestFixture, "Scene instantiation - nested entities", "[scene][instantiate]") {
    const char *source = R"(
        Entity Player @(100, 100) {
            TestHealth: 100

            Entity Weapon @(20, 0) {
                TestSprite: "sword.png"
            }
        }
    )";

    auto ctx = make_context();
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);
    bool result = agentite_scene_instantiate(scene, ecs, &ctx);
    REQUIRE(result == true);

    /* Should have 2 entities total (parent + child) */
    REQUIRE(agentite_scene_get_entity_count(scene) == 2);

    /* But only 1 root */
    ecs_entity_t roots[16];
    size_t root_count = agentite_scene_get_root_entities(scene, roots, 16);
    REQUIRE(root_count == 1);

    agentite_scene_destroy(scene);
}

/* ============================================================================
 * Scene Uninstantiation Tests
 * ============================================================================ */

TEST_CASE_METHOD(SceneTestFixture, "Scene uninstantiation", "[scene][uninstantiate]") {
    const char *source = R"(
        Entity TestEntity {
            TestHealth: { current: 100, max: 100 }
        }
    )";

    auto ctx = make_context();
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);

    /* Instantiate */
    REQUIRE(agentite_scene_instantiate(scene, ecs, &ctx) == true);
    REQUIRE(agentite_scene_get_entity_count(scene) == 1);

    /* Get entity ID before uninstantiation */
    ecs_entity_t entities[16];
    agentite_scene_get_entities(scene, entities, 16);
    ecs_entity_t entity = entities[0];
    REQUIRE(ecs_is_alive(ecs, entity) == true);

    /* Uninstantiate */
    agentite_scene_uninstantiate(scene, ecs);

    SECTION("State changes back to parsed") {
        REQUIRE(agentite_scene_get_state(scene) == AGENTITE_SCENE_PARSED);
    }

    SECTION("Entity count is zero") {
        REQUIRE(agentite_scene_get_entity_count(scene) == 0);
    }

    SECTION("Is not instantiated") {
        REQUIRE(agentite_scene_is_instantiated(scene) == false);
    }

    SECTION("Entity is deleted from world") {
        REQUIRE(ecs_is_alive(ecs, entity) == false);
    }

    agentite_scene_destroy(scene);
}

/* ============================================================================
 * Scene Find Entity Tests
 * ============================================================================ */

TEST_CASE_METHOD(SceneTestFixture, "Scene find entity by name", "[scene][find]") {
    const char *source = R"(
        Entity Player @(100, 100) {
            TestHealth: 100
        }

        Entity Enemy @(300, 100) {
            TestHealth: 50
        }
    )";

    auto ctx = make_context();
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);
    agentite_scene_instantiate(scene, ecs, &ctx);

    SECTION("Find existing entity") {
        ecs_entity_t player = agentite_scene_find_entity(scene, "Player");
        REQUIRE(player != 0);
        REQUIRE(ecs_is_alive(ecs, player) == true);
    }

    SECTION("Find another entity") {
        ecs_entity_t enemy = agentite_scene_find_entity(scene, "Enemy");
        REQUIRE(enemy != 0);
        REQUIRE(ecs_is_alive(ecs, enemy) == true);
    }

    SECTION("Non-existent entity returns 0") {
        ecs_entity_t npc = agentite_scene_find_entity(scene, "NonExistent");
        REQUIRE(npc == 0);
    }

    agentite_scene_destroy(scene);
}

/* ============================================================================
 * Asset Reference Tests
 * ============================================================================ */

TEST_CASE("Scene asset references", "[scene][assets]") {
    const char *source = R"(
        Entity Player {
            TestSprite: "player.png"
        }

        Entity Enemy {
            TestSprite: "enemies/goblin.png"
            prefab: "enemies/goblin.prefab"
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    Agentite_AssetRef refs[32];
    size_t ref_count = agentite_scene_get_asset_refs(scene, refs, 32);

    /* Should find 3 unique asset paths */
    REQUIRE(ref_count == 3);

    agentite_scene_destroy(scene);
}

/* ============================================================================
 * Scene Serialization Tests
 * ============================================================================ */

TEST_CASE("Scene write string", "[scene][write]") {
    const char *source = R"(
        Entity Player @(100, 200) {
            TestHealth: { current: 50, max: 100 }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    char *output = agentite_scene_write_string(scene);
    REQUIRE(output != nullptr);

    /* Output should contain key elements (new format without "Entity" keyword) */
    REQUIRE(strstr(output, "Player") != nullptr);
    REQUIRE(strstr(output, "TestHealth") != nullptr);

    free(output);
    agentite_scene_destroy(scene);
}

TEST_CASE("Scene roundtrip", "[scene][roundtrip]") {
    const char *source = R"(
        Entity Player @(100, 200) {
            TestHealth: { current: 50, max: 100 }
        }

        Entity Enemy @(400, 200) {
            TestHealth: { current: 25, max: 50 }
        }
    )";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;

    /* Parse original */
    Agentite_Scene *scene1 = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene1 != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene1) == 2);

    /* Serialize */
    char *output = agentite_scene_write_string(scene1);
    REQUIRE(output != nullptr);

    /* Parse serialized version */
    Agentite_Scene *scene2 = agentite_scene_load_string(output, 0, "test2", &ctx);
    REQUIRE(scene2 != nullptr);
    REQUIRE(agentite_scene_get_root_count(scene2) == 2);

    free(output);
    agentite_scene_destroy(scene1);
    agentite_scene_destroy(scene2);
}

/* ============================================================================
 * Scene Transition Tests
 * ============================================================================ */

TEST_CASE_METHOD(SceneTestFixture, "Scene transition", "[scene][transition]") {
    /* Note: For file-based tests, we would need actual files.
       This test uses string loading instead. */

    const char *source1 = R"(
        Entity Level1Entity {
            TestHealth: 100
        }
    )";

    const char *source2 = R"(
        Entity Level2Entity {
            TestHealth: 50
        }
    )";

    auto ctx = make_context();
    ecs_world_t *ecs = agentite_ecs_get_world(world);

    /* Load and instantiate first scene manually (simulating transition) */
    Agentite_Scene *scene1 = agentite_scene_load_string(source1, 0, "level1", &ctx);
    REQUIRE(scene1 != nullptr);
    agentite_scene_instantiate(scene1, ecs, &ctx);
    agentite_scene_manager_set_active(scenes, scene1);

    ecs_entity_t entity1 = agentite_scene_find_entity(scene1, "Level1Entity");
    REQUIRE(entity1 != 0);
    REQUIRE(ecs_is_alive(ecs, entity1) == true);

    /* Manually transition to second scene */
    agentite_scene_uninstantiate(scene1, ecs);
    Agentite_Scene *scene2 = agentite_scene_load_string(source2, 0, "level2", &ctx);
    REQUIRE(scene2 != nullptr);
    agentite_scene_instantiate(scene2, ecs, &ctx);
    agentite_scene_manager_set_active(scenes, scene2);

    /* Old entity should be gone */
    REQUIRE(ecs_is_alive(ecs, entity1) == false);

    /* New entity should exist */
    ecs_entity_t entity2 = agentite_scene_find_entity(scene2, "Level2Entity");
    REQUIRE(entity2 != 0);
    REQUIRE(ecs_is_alive(ecs, entity2) == true);

    /* Active scene should be scene2 */
    REQUIRE(agentite_scene_manager_get_active(scenes) == scene2);

    agentite_scene_destroy(scene1);
    agentite_scene_destroy(scene2);
}

/* ============================================================================
 * Scene Properties Tests
 * ============================================================================ */

TEST_CASE("Scene properties", "[scene][properties]") {
    const char *source = "Entity Test { TestHealth: 100 }";

    Agentite_SceneLoadContext ctx = AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT;
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "my_scene", &ctx);
    REQUIRE(scene != nullptr);

    SECTION("Name") {
        REQUIRE(strcmp(agentite_scene_get_name(scene), "my_scene") == 0);
    }

    SECTION("Path is null for string-loaded") {
        REQUIRE(agentite_scene_get_path(scene) == nullptr);
    }

    SECTION("Initial state") {
        REQUIRE(agentite_scene_get_state(scene) == AGENTITE_SCENE_PARSED);
    }

    agentite_scene_destroy(scene);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_CASE("Scene NULL handling", "[scene][null]") {
    REQUIRE(agentite_scene_get_root_count(nullptr) == 0);
    REQUIRE(agentite_scene_get_entity_count(nullptr) == 0);
    REQUIRE(agentite_scene_get_name(nullptr) == nullptr);
    REQUIRE(agentite_scene_get_path(nullptr) == nullptr);
    REQUIRE(agentite_scene_is_instantiated(nullptr) == false);
    REQUIRE(agentite_scene_find_entity(nullptr, "test") == 0);
    REQUIRE(agentite_scene_write_string(nullptr) == nullptr);

    /* These should not crash */
    agentite_scene_destroy(nullptr);
    agentite_scene_uninstantiate(nullptr, nullptr);
}

TEST_CASE_METHOD(SceneTestFixture, "Double instantiation fails", "[scene][edge]") {
    const char *source = "Entity Test { TestHealth: 100 }";

    auto ctx = make_context();
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);

    /* First instantiation succeeds */
    REQUIRE(agentite_scene_instantiate(scene, ecs, &ctx) == true);

    /* Second instantiation fails */
    REQUIRE(agentite_scene_instantiate(scene, ecs, &ctx) == false);

    agentite_scene_destroy(scene);
}

TEST_CASE_METHOD(SceneTestFixture, "Re-instantiation after uninstantiate", "[scene][edge]") {
    const char *source = "Entity Test { TestHealth: 100 }";

    auto ctx = make_context();
    Agentite_Scene *scene = agentite_scene_load_string(source, 0, "test", &ctx);
    REQUIRE(scene != nullptr);

    ecs_world_t *ecs = agentite_ecs_get_world(world);

    /* First cycle */
    REQUIRE(agentite_scene_instantiate(scene, ecs, &ctx) == true);
    REQUIRE(agentite_scene_get_entity_count(scene) == 1);
    agentite_scene_uninstantiate(scene, ecs);
    REQUIRE(agentite_scene_get_entity_count(scene) == 0);

    /* Second cycle should work */
    REQUIRE(agentite_scene_instantiate(scene, ecs, &ctx) == true);
    REQUIRE(agentite_scene_get_entity_count(scene) == 1);

    agentite_scene_destroy(scene);
}
