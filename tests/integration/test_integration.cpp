/*
 * Agentite Integration Tests
 *
 * Tests that verify multiple systems work correctly together.
 * These tests focus on the interactions between subsystems, not just
 * individual unit tests.
 *
 * Note: Tests that require GPU/window are marked and may be skipped in
 * CI environments without display.
 */

#include "catch_amalgamated.hpp"
#include "agentite/ecs.h"
#include "agentite/turn.h"
#include "agentite/resource.h"
#include "agentite/tech.h"
#include "agentite/spatial.h"
#include "agentite/fog.h"
#include "agentite/pathfinding.h"
#include <cstring>

/* ============================================================================
 * ECS + Component Integration Tests
 *
 * Tests the full ECS lifecycle: create world, add entities with multiple
 * components, run systems, and cleanup.
 * ============================================================================ */

TEST_CASE("ECS full lifecycle with multiple entities and components", "[integration][ecs]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);
    agentite_ecs_register_components(world);
    ecs_world_t *ecs = agentite_ecs_get_world(world);

    SECTION("Create, update, and destroy many entities") {
        const int ENTITY_COUNT = 100;
        ecs_entity_t entities[ENTITY_COUNT];

        /* Create phase */
        for (int i = 0; i < ENTITY_COUNT; i++) {
            entities[i] = agentite_ecs_entity_new(world);
            REQUIRE(entities[i] != 0);

            /* Add components */
            C_Position pos = {(float)i * 10.0f, (float)i * 5.0f};
            C_Velocity vel = {1.0f, 0.5f};
            ecs_set_id(ecs, entities[i], ecs_id(C_Position), sizeof(C_Position), &pos);
            ecs_set_id(ecs, entities[i], ecs_id(C_Velocity), sizeof(C_Velocity), &vel);
        }

        /* Verify all entities exist and have components */
        for (int i = 0; i < ENTITY_COUNT; i++) {
            REQUIRE(agentite_ecs_entity_is_alive(world, entities[i]));
            const C_Position *pos = AGENTITE_ECS_GET(world, entities[i], C_Position);
            REQUIRE(pos != nullptr);
            REQUIRE(pos->x == (float)i * 10.0f);
        }

        /* Progress the world (simulates game loop) */
        for (int frame = 0; frame < 60; frame++) {
            agentite_ecs_progress(world, 0.016f);
        }

        /* Delete half the entities */
        for (int i = 0; i < ENTITY_COUNT / 2; i++) {
            agentite_ecs_entity_delete(world, entities[i]);
        }
        agentite_ecs_progress(world, 0.016f); /* Process deletions */

        /* Verify deletions */
        for (int i = 0; i < ENTITY_COUNT / 2; i++) {
            REQUIRE_FALSE(agentite_ecs_entity_is_alive(world, entities[i]));
        }
        for (int i = ENTITY_COUNT / 2; i < ENTITY_COUNT; i++) {
            REQUIRE(agentite_ecs_entity_is_alive(world, entities[i]));
        }
    }

    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Turn System + Resource System Integration Tests
 *
 * Tests the interaction between turn progression and resource management.
 * ============================================================================ */

/* Callback data for turn system integration */
struct TurnCallbackData {
    int world_updates;
    int event_ticks;
    int player_inputs;
    int resolutions;
    int end_checks;
    Agentite_Resource *gold;
    Agentite_Resource *food;
};

static void world_update_cb(void *userdata, int /*turn_number*/) {
    TurnCallbackData *data = (TurnCallbackData *)userdata;
    data->world_updates++;
    /* Simulate resource generation */
    if (data->gold) {
        agentite_resource_tick(data->gold);
    }
    if (data->food) {
        agentite_resource_tick(data->food);
    }
}

static void events_cb(void *userdata, int /*turn_number*/) {
    TurnCallbackData *data = (TurnCallbackData *)userdata;
    data->event_ticks++;
}

static void player_input_cb(void *userdata, int /*turn_number*/) {
    TurnCallbackData *data = (TurnCallbackData *)userdata;
    data->player_inputs++;
}

static void resolution_cb(void *userdata, int /*turn_number*/) {
    TurnCallbackData *data = (TurnCallbackData *)userdata;
    data->resolutions++;
    /* Apply food consumption */
    if (data->food) {
        agentite_resource_spend(data->food, 5); /* Consume 5 food per turn */
    }
}

static void end_check_cb(void *userdata, int /*turn_number*/) {
    TurnCallbackData *data = (TurnCallbackData *)userdata;
    data->end_checks++;
}

TEST_CASE("Turn system drives resource updates", "[integration][turn][resource]") {
    /* Setup resources */
    Agentite_Resource gold, food;
    agentite_resource_init(&gold, 100, 1000, 10);  /* Start 100, max 1000, +10/turn */
    agentite_resource_init(&food, 50, 500, 8);     /* Start 50, max 500, +8/turn */

    /* Setup turn manager */
    Agentite_TurnManager tm;
    agentite_turn_init(&tm);

    TurnCallbackData cb_data = {0, 0, 0, 0, 0, &gold, &food};

    agentite_turn_set_callback(&tm, AGENTITE_PHASE_WORLD_UPDATE, world_update_cb, &cb_data);
    agentite_turn_set_callback(&tm, AGENTITE_PHASE_EVENTS, events_cb, &cb_data);
    agentite_turn_set_callback(&tm, AGENTITE_PHASE_PLAYER_INPUT, player_input_cb, &cb_data);
    agentite_turn_set_callback(&tm, AGENTITE_PHASE_RESOLUTION, resolution_cb, &cb_data);
    agentite_turn_set_callback(&tm, AGENTITE_PHASE_END_CHECK, end_check_cb, &cb_data);

    SECTION("Complete 10 full turns") {
        int completed_turns = 0;
        for (int i = 0; i < 50; i++) { /* 5 phases * 10 turns */
            bool turn_done = agentite_turn_advance(&tm);
            if (turn_done) {
                completed_turns++;
            }
        }

        /* Verify callbacks were called correct number of times */
        REQUIRE(completed_turns == 10);
        REQUIRE(cb_data.world_updates == 10);
        REQUIRE(cb_data.event_ticks == 10);
        REQUIRE(cb_data.player_inputs == 10);
        REQUIRE(cb_data.resolutions == 10);
        REQUIRE(cb_data.end_checks == 10);

        /* Verify resource changes */
        /* Gold: 100 + (10 * 10) = 200 */
        REQUIRE(gold.current == 200);
        /* Food: 50 + (8 * 10) - (5 * 10) = 50 + 80 - 50 = 80 */
        REQUIRE(food.current == 80);
    }

    SECTION("Resource caps are respected over many turns") {
        /* Run many turns to hit gold cap */
        for (int i = 0; i < 500; i++) {
            agentite_turn_advance(&tm);
        }

        /* Gold should be capped at 1000 */
        REQUIRE(gold.current <= 1000);
    }
}

/* ============================================================================
 * Tech Tree + Resource Integration Tests
 *
 * Tests researching technologies that cost resources.
 * ============================================================================ */

TEST_CASE("Tech tree research with resource costs", "[integration][tech][resource]") {
    Agentite_TechTree *tree = agentite_tech_create();
    REQUIRE(tree != nullptr);

    /* Define technologies using proper struct initialization */
    Agentite_TechDef tech1 = {};
    strncpy(tech1.id, "basic_tools", sizeof(tech1.id) - 1);
    strncpy(tech1.name, "Basic Tools", sizeof(tech1.name) - 1);
    tech1.research_cost = 50;
    tech1.prereq_count = 0;
    agentite_tech_register(tree, &tech1);

    Agentite_TechDef tech2 = {};
    strncpy(tech2.id, "advanced_tools", sizeof(tech2.id) - 1);
    strncpy(tech2.name, "Advanced Tools", sizeof(tech2.name) - 1);
    tech2.research_cost = 100;
    tech2.prereq_count = 1;
    strncpy(tech2.prerequisites[0], "basic_tools", sizeof(tech2.prerequisites[0]) - 1);
    agentite_tech_register(tree, &tech2);

    Agentite_TechState state;
    agentite_tech_state_init(&state);

    /* Setup research points resource */
    Agentite_Resource research_points;
    agentite_resource_init(&research_points, 0, 10000, 20);

    SECTION("Research unlocks prerequisites") {
        /* Can't research advanced without basic */
        bool can_start_advanced = agentite_tech_can_research(tree, &state, "advanced_tools");
        REQUIRE_FALSE(can_start_advanced);

        /* Research basic_tools */
        REQUIRE(agentite_tech_start_research(tree, &state, "basic_tools"));

        /* Add points until complete */
        for (int turn = 0; turn < 10; turn++) {
            agentite_resource_tick(&research_points);
            int points = research_points.current;
            if (agentite_tech_add_points(tree, &state, points)) {
                agentite_resource_set(&research_points, 0);
                break;
            }
            agentite_resource_set(&research_points, 0);
        }

        REQUIRE(agentite_tech_is_researched(tree, &state, "basic_tools"));

        /* Now can research advanced */
        can_start_advanced = agentite_tech_can_research(tree, &state, "advanced_tools");
        REQUIRE(can_start_advanced);
    }

    agentite_tech_destroy(tree);
}

/* ============================================================================
 * Spatial Index + ECS Integration Tests
 *
 * Tests using spatial indexing with ECS entities.
 * ============================================================================ */

TEST_CASE("Spatial index with ECS entities", "[integration][spatial][ecs]") {
    Agentite_World *world = agentite_ecs_init();
    REQUIRE(world != nullptr);
    agentite_ecs_register_components(world);
    ecs_world_t *ecs = agentite_ecs_get_world(world);

    /* Create spatial index with hash table capacity */
    Agentite_SpatialIndex *spatial = agentite_spatial_create(256);
    REQUIRE(spatial != nullptr);

    SECTION("Add entities and query by position") {
        /* Create entities at various positions */
        ecs_entity_t e1 = agentite_ecs_entity_new(world);
        ecs_entity_t e2 = agentite_ecs_entity_new(world);
        ecs_entity_t e3 = agentite_ecs_entity_new(world);

        /* Set positions and add to spatial index */
        C_Position pos1 = {50.0f, 50.0f};
        C_Position pos2 = {55.0f, 55.0f};
        C_Position pos3 = {500.0f, 500.0f};

        ecs_set_id(ecs, e1, ecs_id(C_Position), sizeof(C_Position), &pos1);
        ecs_set_id(ecs, e2, ecs_id(C_Position), sizeof(C_Position), &pos2);
        ecs_set_id(ecs, e3, ecs_id(C_Position), sizeof(C_Position), &pos3);

        agentite_spatial_add(spatial, (int)pos1.x, (int)pos1.y, (uint32_t)e1);
        agentite_spatial_add(spatial, (int)pos2.x, (int)pos2.y, (uint32_t)e2);
        agentite_spatial_add(spatial, (int)pos3.x, (int)pos3.y, (uint32_t)e3);

        /* Query area around (50, 50) with radius 20 */
        Agentite_SpatialQueryResult results[10];
        int found = agentite_spatial_query_radius(spatial, 50, 50, 20, results, 10);

        /* Should find e1 and e2 but not e3 */
        REQUIRE(found == 2);
        bool found_e1 = false, found_e2 = false;
        for (int i = 0; i < found; i++) {
            if (results[i].entity_id == (uint32_t)e1) found_e1 = true;
            if (results[i].entity_id == (uint32_t)e2) found_e2 = true;
        }
        REQUIRE(found_e1);
        REQUIRE(found_e2);
    }

    SECTION("Move entities updates spatial index") {
        ecs_entity_t e = agentite_ecs_entity_new(world);

        C_Position pos = {100.0f, 100.0f};
        ecs_set_id(ecs, e, ecs_id(C_Position), sizeof(C_Position), &pos);
        agentite_spatial_add(spatial, 100, 100, (uint32_t)e);

        /* Query original position */
        Agentite_SpatialQueryResult results[10];
        int found = agentite_spatial_query_radius(spatial, 100, 100, 10, results, 10);
        REQUIRE(found == 1);

        /* Move entity */
        agentite_spatial_move(spatial, 100, 100, 800, 800, (uint32_t)e);
        pos = {800.0f, 800.0f};
        ecs_set_id(ecs, e, ecs_id(C_Position), sizeof(C_Position), &pos);

        /* Query old position should be empty */
        found = agentite_spatial_query_radius(spatial, 100, 100, 10, results, 10);
        REQUIRE(found == 0);

        /* Query new position should find entity */
        found = agentite_spatial_query_radius(spatial, 800, 800, 10, results, 10);
        REQUIRE(found == 1);
        REQUIRE(results[0].entity_id == (uint32_t)e);
    }

    agentite_spatial_destroy(spatial);
    agentite_ecs_shutdown(world);
}

/* ============================================================================
 * Fog of War Integration Tests
 *
 * Tests visibility system with vision sources.
 * ============================================================================ */

TEST_CASE("Fog of war with vision sources", "[integration][fog]") {
    /* Create 50x50 fog grid */
    Agentite_FogOfWar *fog = agentite_fog_create(50, 50);
    REQUIRE(fog != nullptr);

    SECTION("Vision sources reveal tiles") {
        /* Initially all tiles are unexplored */
        REQUIRE(agentite_fog_is_unexplored(fog, 25, 25));

        /* Add a vision source at (20, 20) with radius 10 */
        Agentite_VisionSource source = agentite_fog_add_source(fog, 20, 20, 10);
        REQUIRE(source != AGENTITE_VISION_SOURCE_INVALID);

        /* Update fog */
        agentite_fog_update(fog);

        /* Tiles near the source should now be visible */
        REQUIRE(agentite_fog_is_visible(fog, 20, 20));
        REQUIRE(agentite_fog_is_visible(fog, 25, 20));

        /* Tiles far from source should remain unexplored */
        REQUIRE_FALSE(agentite_fog_is_visible(fog, 45, 45));
    }

    SECTION("Moving vision source updates visibility") {
        Agentite_VisionSource source = agentite_fog_add_source(fog, 10, 10, 5);
        agentite_fog_update(fog);

        REQUIRE(agentite_fog_is_visible(fog, 10, 10));

        /* Move source to new position */
        agentite_fog_move_source(fog, source, 40, 40);
        agentite_fog_update(fog);

        /* Old position should now be explored but not visible */
        REQUIRE(agentite_fog_is_explored(fog, 10, 10));
        REQUIRE_FALSE(agentite_fog_is_visible(fog, 10, 10));

        /* New position should be visible */
        REQUIRE(agentite_fog_is_visible(fog, 40, 40));
    }

    SECTION("Removing vision source hides area") {
        Agentite_VisionSource source = agentite_fog_add_source(fog, 25, 25, 8);
        agentite_fog_update(fog);

        REQUIRE(agentite_fog_is_visible(fog, 25, 25));

        /* Remove the source */
        agentite_fog_remove_source(fog, source);
        agentite_fog_update(fog);

        /* Area should now be explored but not visible */
        REQUIRE(agentite_fog_is_explored(fog, 25, 25));
        REQUIRE_FALSE(agentite_fog_is_visible(fog, 25, 25));
    }

    agentite_fog_destroy(fog);
}

/* ============================================================================
 * Pathfinding Integration Tests
 *
 * Tests pathfinding with blocked cells.
 * ============================================================================ */

TEST_CASE("Pathfinding around obstacles", "[integration][pathfinding]") {
    /* Create 20x20 pathfinding grid */
    Agentite_Pathfinder *pf = agentite_pathfinder_create(20, 20);
    REQUIRE(pf != nullptr);

    SECTION("Path around blocked cells") {
        /* Block a wall across the middle (set walkable to false) */
        for (int x = 5; x < 15; x++) {
            agentite_pathfinder_set_walkable(pf, x, 10, false);
        }
        /* Leave a gap at x=15 */

        /* Find path from (10, 5) to (10, 15) */
        Agentite_Path *path = agentite_pathfinder_find(pf, 10, 5, 10, 15);
        REQUIRE(path != nullptr);
        REQUIRE(path->length > 0);

        /* Path should go around the wall */
        bool crosses_wall = false;
        for (int i = 0; i < path->length; i++) {
            if (path->points[i].y == 10 &&
                path->points[i].x >= 5 && path->points[i].x < 15) {
                crosses_wall = true;
            }
        }
        REQUIRE_FALSE(crosses_wall);

        agentite_path_destroy(path);
    }

    SECTION("No path when completely blocked") {
        /* Block entire row */
        for (int x = 0; x < 20; x++) {
            agentite_pathfinder_set_walkable(pf, x, 10, false);
        }

        /* Try to find path across blocked row */
        Agentite_Path *path = agentite_pathfinder_find(pf, 10, 5, 10, 15);
        REQUIRE(path == nullptr); /* No path possible */
    }

    agentite_pathfinder_destroy(pf);
}

/* ============================================================================
 * Full Strategy Game Loop Integration Test
 *
 * Simulates a complete turn-based strategy game loop with:
 * - Turn management
 * - Resource production
 * - Tech research
 * ============================================================================ */

struct GameState {
    Agentite_TurnManager turn_manager;
    Agentite_Resource gold;
    Agentite_Resource science;
    Agentite_TechTree *tech_tree;
    Agentite_TechState tech_state;
    int turn_count;
};

static void game_world_update(void *userdata, int /*turn_number*/) {
    GameState *gs = (GameState *)userdata;
    /* Resource production */
    agentite_resource_tick(&gs->gold);
    agentite_resource_tick(&gs->science);
}

static void game_resolution(void *userdata, int /*turn_number*/) {
    GameState *gs = (GameState *)userdata;

    /* Apply science to research if there's active research */
    if (gs->tech_state.active_count > 0) {
        int science = gs->science.current;
        agentite_tech_add_points(gs->tech_tree, &gs->tech_state, science);
        agentite_resource_set(&gs->science, 0);
    }
}

static void game_end_check(void *userdata, int /*turn_number*/) {
    GameState *gs = (GameState *)userdata;
    gs->turn_count++;
}

TEST_CASE("Full strategy game loop", "[integration][strategy]") {
    GameState gs = {};

    /* Initialize all systems */
    agentite_turn_init(&gs.turn_manager);
    agentite_resource_init(&gs.gold, 100, 10000, 25);   /* +25 gold/turn */
    agentite_resource_init(&gs.science, 0, 10000, 15);  /* +15 science/turn */

    gs.tech_tree = agentite_tech_create();
    REQUIRE(gs.tech_tree != nullptr);

    /* Register a tech */
    Agentite_TechDef farming = {};
    strncpy(farming.id, "farming", sizeof(farming.id) - 1);
    strncpy(farming.name, "Farming", sizeof(farming.name) - 1);
    farming.research_cost = 60;
    agentite_tech_register(gs.tech_tree, &farming);

    agentite_tech_state_init(&gs.tech_state);

    /* Set up callbacks */
    agentite_turn_set_callback(&gs.turn_manager, AGENTITE_PHASE_WORLD_UPDATE,
                               game_world_update, &gs);
    agentite_turn_set_callback(&gs.turn_manager, AGENTITE_PHASE_RESOLUTION,
                               game_resolution, &gs);
    agentite_turn_set_callback(&gs.turn_manager, AGENTITE_PHASE_END_CHECK,
                               game_end_check, &gs);

    /* Start researching */
    REQUIRE(agentite_tech_start_research(gs.tech_tree, &gs.tech_state, "farming"));

    SECTION("Play 10 turns and verify progression") {
        /* Run 10 complete turns (5 phases each) */
        for (int i = 0; i < 50; i++) {
            agentite_turn_advance(&gs.turn_manager);
        }

        REQUIRE(gs.turn_count == 10);

        /* Gold: 100 + (25 * 10) = 350 */
        REQUIRE(gs.gold.current == 350);

        /* Research should be complete (60 cost, 15/turn for 10 turns = 150 > 60) */
        REQUIRE(agentite_tech_is_researched(gs.tech_tree, &gs.tech_state, "farming"));
    }

    /* Cleanup */
    agentite_tech_destroy(gs.tech_tree);
}
