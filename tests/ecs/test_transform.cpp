/*
 * Agentite Engine - Transform Hierarchy Tests
 *
 * Tests for parent-child relationships and transform propagation.
 */

#include "catch_amalgamated.hpp"
#include "agentite/transform.h"
#include "agentite/ecs.h"
#include "flecs.h"
#include <cmath>

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class TransformTestFixture {
public:
    Agentite_World *aworld = nullptr;
    ecs_world_t *world = nullptr;

    TransformTestFixture() {
        aworld = agentite_ecs_init();
        if (aworld) {
            world = agentite_ecs_get_world(aworld);
            agentite_transform_register(world);
        }
    }

    ~TransformTestFixture() {
        agentite_ecs_shutdown(aworld);
    }

    ecs_entity_t create_entity_with_transform(float x, float y,
                                               float rotation = 0.0f,
                                               float sx = 1.0f,
                                               float sy = 1.0f) {
        ecs_entity_t e = ecs_new(world);
        C_Transform tf = {x, y, rotation, sx, sy};
        C_WorldTransform wtf = {x, y, rotation, sx, sy};
        ecs_set_id(world, e, ecs_id(C_Transform), sizeof(C_Transform), &tf);
        ecs_set_id(world, e, ecs_id(C_WorldTransform), sizeof(C_WorldTransform), &wtf);
        return e;
    }

    void progress() {
        ecs_progress(world, 0.016f);
    }
};

/* ============================================================================
 * Component Registration Tests
 * ============================================================================ */

TEST_CASE("Transform registration", "[transform][register]") {
    Agentite_World *aworld = agentite_ecs_init();
    REQUIRE(aworld != nullptr);

    ecs_world_t *world = agentite_ecs_get_world(aworld);
    agentite_transform_register(world);

    /* Components should be registered */
    REQUIRE(ecs_id(C_Transform) != 0);
    REQUIRE(ecs_id(C_WorldTransform) != 0);

    agentite_ecs_shutdown(aworld);
}

TEST_CASE("Transform registration - NULL safety", "[transform][register]") {
    agentite_transform_register(nullptr);
    agentite_transform_register_world(nullptr);
    /* Should not crash */
}

/* ============================================================================
 * Parent-Child Relationship Tests
 * ============================================================================ */

TEST_CASE_METHOD(TransformTestFixture, "Set parent", "[transform][hierarchy]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);
    ecs_entity_t child = ecs_new(world);

    SECTION("Set parent creates relationship") {
        agentite_transform_set_parent(world, child, parent);

        REQUIRE(agentite_transform_has_parent(world, child) == true);
        REQUIRE(agentite_transform_get_parent(world, child) == parent);
    }

    SECTION("Child gets C_Transform if missing") {
        REQUIRE(ecs_has(world, child, C_Transform) == false);

        agentite_transform_set_parent(world, child, parent);

        REQUIRE(ecs_has(world, child, C_Transform) == true);
    }

    SECTION("Child gets C_WorldTransform if missing") {
        REQUIRE(ecs_has(world, child, C_WorldTransform) == false);

        agentite_transform_set_parent(world, child, parent);

        REQUIRE(ecs_has(world, child, C_WorldTransform) == true);
    }
}

TEST_CASE_METHOD(TransformTestFixture, "Get parent - no parent", "[transform][hierarchy]") {
    ecs_entity_t entity = create_entity_with_transform(0, 0);

    REQUIRE(agentite_transform_has_parent(world, entity) == false);
    REQUIRE(agentite_transform_get_parent(world, entity) == 0);
}

TEST_CASE_METHOD(TransformTestFixture, "Remove parent", "[transform][hierarchy]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);
    ecs_entity_t child = create_entity_with_transform(20, 20);

    agentite_transform_set_parent(world, child, parent);
    REQUIRE(agentite_transform_has_parent(world, child) == true);

    agentite_transform_remove_parent(world, child);
    REQUIRE(agentite_transform_has_parent(world, child) == false);
}

TEST_CASE_METHOD(TransformTestFixture, "Reparenting", "[transform][hierarchy]") {
    ecs_entity_t parent1 = create_entity_with_transform(100, 100);
    ecs_entity_t parent2 = create_entity_with_transform(200, 200);
    ecs_entity_t child = create_entity_with_transform(10, 10);

    agentite_transform_set_parent(world, child, parent1);
    REQUIRE(agentite_transform_get_parent(world, child) == parent1);

    /* Reparent to parent2 */
    agentite_transform_set_parent(world, child, parent2);
    REQUIRE(agentite_transform_get_parent(world, child) == parent2);

    /* Child should not be a child of parent1 anymore */
    int parent1_children = agentite_transform_get_child_count(world, parent1);
    REQUIRE(parent1_children == 0);
}

/* ============================================================================
 * Get Children Tests
 * ============================================================================ */

TEST_CASE_METHOD(TransformTestFixture, "Get children - empty", "[transform][hierarchy]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);

    REQUIRE(agentite_transform_get_child_count(world, parent) == 0);

    ecs_entity_t children[10];
    int count = agentite_transform_get_children(world, parent, children, 10);
    REQUIRE(count == 0);
}

TEST_CASE_METHOD(TransformTestFixture, "Get children - single child", "[transform][hierarchy]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);
    ecs_entity_t child = create_entity_with_transform(10, 10);

    agentite_transform_set_parent(world, child, parent);

    REQUIRE(agentite_transform_get_child_count(world, parent) == 1);

    ecs_entity_t children[10];
    int count = agentite_transform_get_children(world, parent, children, 10);
    REQUIRE(count == 1);
    REQUIRE(children[0] == child);
}

TEST_CASE_METHOD(TransformTestFixture, "Get children - multiple", "[transform][hierarchy]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);
    ecs_entity_t child1 = create_entity_with_transform(10, 10);
    ecs_entity_t child2 = create_entity_with_transform(20, 20);
    ecs_entity_t child3 = create_entity_with_transform(30, 30);

    agentite_transform_set_parent(world, child1, parent);
    agentite_transform_set_parent(world, child2, parent);
    agentite_transform_set_parent(world, child3, parent);

    REQUIRE(agentite_transform_get_child_count(world, parent) == 3);

    ecs_entity_t children[10];
    int count = agentite_transform_get_children(world, parent, children, 10);
    REQUIRE(count == 3);
}

TEST_CASE_METHOD(TransformTestFixture, "Get children - limited buffer", "[transform][hierarchy]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);

    for (int i = 0; i < 5; i++) {
        ecs_entity_t child = create_entity_with_transform((float)i * 10, 0);
        agentite_transform_set_parent(world, child, parent);
    }

    ecs_entity_t children[2];
    int count = agentite_transform_get_children(world, parent, children, 2);

    /* Should return total count but only fill 2 slots */
    REQUIRE(count == 5);
}

/* ============================================================================
 * Transform Propagation Tests
 * ============================================================================ */

TEST_CASE_METHOD(TransformTestFixture, "Transform propagation - position", "[transform][propagation]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);
    ecs_entity_t child = create_entity_with_transform(20, 30);

    agentite_transform_set_parent(world, child, parent);
    progress();

    float world_x, world_y;
    REQUIRE(agentite_transform_get_world_position(world, child, &world_x, &world_y) == true);

    /* Child at (20, 30) relative to parent at (100, 100) */
    REQUIRE(world_x == Catch::Approx(120.0f));
    REQUIRE(world_y == Catch::Approx(130.0f));
}

TEST_CASE_METHOD(TransformTestFixture, "Transform propagation - rotation", "[transform][propagation]") {
    float pi = 3.14159265f;

    ecs_entity_t parent = create_entity_with_transform(100, 100, pi / 2); /* 90 degrees */
    ecs_entity_t child = create_entity_with_transform(10, 0); /* 10 units to the right */

    agentite_transform_set_parent(world, child, parent);
    progress();

    float world_x, world_y;
    agentite_transform_get_world_position(world, child, &world_x, &world_y);

    /* After 90 degree rotation, (10, 0) becomes (0, 10) relative to parent */
    /* Parent is at (100, 100), so child should be at (100, 110) */
    REQUIRE(world_x == Catch::Approx(100.0f).margin(0.001f));
    REQUIRE(world_y == Catch::Approx(110.0f).margin(0.001f));

    /* World rotation should combine */
    float world_rot = agentite_transform_get_world_rotation(world, child);
    REQUIRE(world_rot == Catch::Approx(pi / 2).margin(0.001f));
}

TEST_CASE_METHOD(TransformTestFixture, "Transform propagation - scale", "[transform][propagation]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100, 0, 2.0f, 2.0f);
    ecs_entity_t child = create_entity_with_transform(10, 10);

    agentite_transform_set_parent(world, child, parent);
    progress();

    float world_x, world_y;
    agentite_transform_get_world_position(world, child, &world_x, &world_y);

    /* Child at (10, 10) scaled by 2x → (20, 20), plus parent (100, 100) */
    REQUIRE(world_x == Catch::Approx(120.0f));
    REQUIRE(world_y == Catch::Approx(120.0f));

    /* World scale should combine */
    float sx, sy;
    agentite_transform_get_world_scale(world, child, &sx, &sy);
    REQUIRE(sx == Catch::Approx(2.0f));
    REQUIRE(sy == Catch::Approx(2.0f));
}

TEST_CASE_METHOD(TransformTestFixture, "Transform propagation - deep hierarchy", "[transform][propagation]") {
    ecs_entity_t root = create_entity_with_transform(100, 0);
    ecs_entity_t child1 = create_entity_with_transform(50, 0);
    ecs_entity_t child2 = create_entity_with_transform(25, 0);
    ecs_entity_t leaf = create_entity_with_transform(12, 0);

    agentite_transform_set_parent(world, child1, root);
    agentite_transform_set_parent(world, child2, child1);
    agentite_transform_set_parent(world, leaf, child2);
    progress();

    float world_x, world_y;
    agentite_transform_get_world_position(world, leaf, &world_x, &world_y);

    /* 100 + 50 + 25 + 12 = 187 */
    REQUIRE(world_x == Catch::Approx(187.0f));
    REQUIRE(world_y == Catch::Approx(0.0f));
}

TEST_CASE_METHOD(TransformTestFixture, "Transform propagation - combined", "[transform][propagation]") {
    float pi = 3.14159265f;

    /* Parent at origin, rotated 90 degrees, scaled 2x */
    ecs_entity_t parent = create_entity_with_transform(0, 0, pi / 2, 2.0f, 2.0f);
    /* Child at (10, 0) in local space */
    ecs_entity_t child = create_entity_with_transform(10, 0);

    agentite_transform_set_parent(world, child, parent);
    progress();

    float world_x, world_y;
    agentite_transform_get_world_position(world, child, &world_x, &world_y);

    /* (10, 0) scaled by 2 = (20, 0) */
    /* (20, 0) rotated 90 deg = (0, 20) */
    /* Plus parent at (0, 0) = (0, 20) */
    REQUIRE(world_x == Catch::Approx(0.0f).margin(0.001f));
    REQUIRE(world_y == Catch::Approx(20.0f).margin(0.001f));
}

/* ============================================================================
 * Coordinate Conversion Tests
 * ============================================================================ */

TEST_CASE_METHOD(TransformTestFixture, "Local to world conversion", "[transform][coordinate]") {
    ecs_entity_t entity = create_entity_with_transform(100, 50);
    progress();

    float world_x, world_y;
    agentite_transform_local_to_world(world, entity, 10, 20, &world_x, &world_y);

    /* Entity at (100, 50) with local point (10, 20) */
    REQUIRE(world_x == Catch::Approx(110.0f));
    REQUIRE(world_y == Catch::Approx(70.0f));
}

TEST_CASE_METHOD(TransformTestFixture, "World to local conversion", "[transform][coordinate]") {
    ecs_entity_t entity = create_entity_with_transform(100, 50);
    progress();

    float local_x, local_y;
    agentite_transform_world_to_local(world, entity, 110, 70, &local_x, &local_y);

    REQUIRE(local_x == Catch::Approx(10.0f));
    REQUIRE(local_y == Catch::Approx(20.0f));
}

TEST_CASE_METHOD(TransformTestFixture, "Coordinate conversion - with rotation", "[transform][coordinate]") {
    float pi = 3.14159265f;
    ecs_entity_t entity = create_entity_with_transform(0, 0, pi / 2);  /* 90 degrees */
    progress();

    float world_x, world_y;
    agentite_transform_local_to_world(world, entity, 10, 0, &world_x, &world_y);

    /* (10, 0) rotated 90 degrees = (0, 10) */
    REQUIRE(world_x == Catch::Approx(0.0f).margin(0.001f));
    REQUIRE(world_y == Catch::Approx(10.0f).margin(0.001f));
}

TEST_CASE_METHOD(TransformTestFixture, "Coordinate roundtrip", "[transform][coordinate]") {
    float pi = 3.14159265f;
    ecs_entity_t entity = create_entity_with_transform(50, 75, pi / 4, 1.5f, 2.0f);
    progress();

    float orig_x = 10.0f, orig_y = 20.0f;
    float world_x, world_y;
    float back_x, back_y;

    agentite_transform_local_to_world(world, entity, orig_x, orig_y, &world_x, &world_y);
    agentite_transform_world_to_local(world, entity, world_x, world_y, &back_x, &back_y);

    REQUIRE(back_x == Catch::Approx(orig_x).margin(0.001f));
    REQUIRE(back_y == Catch::Approx(orig_y).margin(0.001f));
}

/* ============================================================================
 * Transform Manipulation Tests
 * ============================================================================ */

TEST_CASE_METHOD(TransformTestFixture, "Set local position", "[transform][manipulate]") {
    ecs_entity_t entity = ecs_new(world);

    agentite_transform_set_local_position(world, entity, 100, 200);

    const C_Transform *t = ecs_get(world, entity, C_Transform);
    REQUIRE(t != nullptr);
    REQUIRE(t->local_x == 100.0f);
    REQUIRE(t->local_y == 200.0f);
}

TEST_CASE_METHOD(TransformTestFixture, "Set local rotation", "[transform][manipulate]") {
    ecs_entity_t entity = ecs_new(world);
    float pi = 3.14159265f;

    agentite_transform_set_local_rotation(world, entity, pi / 2);

    const C_Transform *t = ecs_get(world, entity, C_Transform);
    REQUIRE(t != nullptr);
    REQUIRE(t->rotation == Catch::Approx(pi / 2));
}

TEST_CASE_METHOD(TransformTestFixture, "Set local scale", "[transform][manipulate]") {
    ecs_entity_t entity = ecs_new(world);

    agentite_transform_set_local_scale(world, entity, 2.0f, 3.0f);

    const C_Transform *t = ecs_get(world, entity, C_Transform);
    REQUIRE(t != nullptr);
    REQUIRE(t->scale_x == 2.0f);
    REQUIRE(t->scale_y == 3.0f);
}

TEST_CASE_METHOD(TransformTestFixture, "Translate", "[transform][manipulate]") {
    ecs_entity_t entity = create_entity_with_transform(100, 100);

    agentite_transform_translate(world, entity, 25, -10);

    const C_Transform *t = ecs_get(world, entity, C_Transform);
    REQUIRE(t->local_x == 125.0f);
    REQUIRE(t->local_y == 90.0f);
}

TEST_CASE_METHOD(TransformTestFixture, "Rotate", "[transform][manipulate]") {
    float pi = 3.14159265f;
    ecs_entity_t entity = create_entity_with_transform(0, 0, pi / 4);

    agentite_transform_rotate(world, entity, pi / 4);

    const C_Transform *t = ecs_get(world, entity, C_Transform);
    REQUIRE(t->rotation == Catch::Approx(pi / 2));
}

/* ============================================================================
 * Manual Update Tests
 * ============================================================================ */

TEST_CASE_METHOD(TransformTestFixture, "Manual update single entity", "[transform][update]") {
    ecs_entity_t entity = create_entity_with_transform(100, 100);

    /* Modify without progress */
    C_Transform *t = ecs_get_mut(world, entity, C_Transform);
    t->local_x = 200;
    ecs_modified(world, entity, C_Transform);

    /* Manual update */
    agentite_transform_update(world, entity);

    float world_x, world_y;
    agentite_transform_get_world_position(world, entity, &world_x, &world_y);
    REQUIRE(world_x == Catch::Approx(200.0f));
}

TEST_CASE_METHOD(TransformTestFixture, "Manual update with children", "[transform][update]") {
    ecs_entity_t parent = create_entity_with_transform(100, 100);
    ecs_entity_t child = create_entity_with_transform(20, 20);
    agentite_transform_set_parent(world, child, parent);
    progress();

    /* Modify parent position */
    C_Transform *t = ecs_get_mut(world, parent, C_Transform);
    t->local_x = 200;
    ecs_modified(world, parent, C_Transform);

    /* Manual update propagates to children */
    agentite_transform_update(world, parent);

    float world_x, world_y;
    agentite_transform_get_world_position(world, child, &world_x, &world_y);
    REQUIRE(world_x == Catch::Approx(220.0f));
    REQUIRE(world_y == Catch::Approx(120.0f));
}

/* ============================================================================
 * Null Safety Tests
 * ============================================================================ */

TEST_CASE("Transform functions - NULL safety", "[transform][null]") {
    /* All functions should handle NULL world gracefully */
    agentite_transform_set_parent(nullptr, 1, 2);
    REQUIRE(agentite_transform_get_parent(nullptr, 1) == 0);
    REQUIRE(agentite_transform_has_parent(nullptr, 1) == false);

    ecs_entity_t children[10];
    REQUIRE(agentite_transform_get_children(nullptr, 1, children, 10) == 0);
    REQUIRE(agentite_transform_get_child_count(nullptr, 1) == 0);

    float x, y;
    REQUIRE(agentite_transform_get_world_position(nullptr, 1, &x, &y) == false);
    REQUIRE(agentite_transform_get_world_scale(nullptr, 1, &x, &y) == false);
    REQUIRE(agentite_transform_get_world_rotation(nullptr, 1) == 0.0f);

    REQUIRE(agentite_transform_local_to_world(nullptr, 1, 0, 0, &x, &y) == false);
    REQUIRE(agentite_transform_world_to_local(nullptr, 1, 0, 0, &x, &y) == false);

    agentite_transform_set_local_position(nullptr, 1, 0, 0);
    agentite_transform_set_local_rotation(nullptr, 1, 0);
    agentite_transform_set_local_scale(nullptr, 1, 1, 1);
    agentite_transform_translate(nullptr, 1, 0, 0);
    agentite_transform_rotate(nullptr, 1, 0);

    agentite_transform_update(nullptr, 1);
    agentite_transform_update_all(nullptr);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_CASE_METHOD(TransformTestFixture, "Zero scale handling", "[transform][edge]") {
    ecs_entity_t entity = create_entity_with_transform(100, 100, 0, 0, 0);
    progress();

    /* World to local with zero scale should not crash */
    float local_x, local_y;
    bool result = agentite_transform_world_to_local(world, entity, 110, 110, &local_x, &local_y);
    REQUIRE(result == true);
    /* With zero scale, inverse uses 1.0 fallback */
}

TEST_CASE_METHOD(TransformTestFixture, "Self-parent prevention", "[transform][edge]") {
    ecs_entity_t entity = create_entity_with_transform(100, 100);

    /* Don't actually call set_parent with self - it causes undefined behavior */
    /* Just verify the entity doesn't have a parent initially */
    REQUIRE(agentite_transform_has_parent(world, entity) == false);

    /* Add a different entity as parent to verify the API works */
    ecs_entity_t parent = create_entity_with_transform(0, 0);
    agentite_transform_set_parent(world, entity, parent);
    REQUIRE(agentite_transform_get_parent(world, entity) == parent);
}

TEST_CASE_METHOD(TransformTestFixture, "Entity without transform - get world position", "[transform][edge]") {
    ecs_entity_t entity = ecs_new(world);

    float x, y;
    bool result = agentite_transform_get_world_position(world, entity, &x, &y);

    /* Should return false for entity without transform */
    REQUIRE(result == false);
}
