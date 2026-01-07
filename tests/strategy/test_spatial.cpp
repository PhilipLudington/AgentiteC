/*
 * Agentite Spatial Index Tests
 *
 * Tests for the spatial hash index system including basic operations,
 * hash collisions, rehashing, region queries, and edge cases.
 */

#include "catch_amalgamated.hpp"
#include "agentite/spatial.h"
#include <cstdlib>
#include <vector>

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

TEST_CASE("Spatial index creation and destruction", "[spatial][lifecycle]") {
    SECTION("Create with valid capacity") {
        Agentite_SpatialIndex *index = agentite_spatial_create(64);
        REQUIRE(index != nullptr);
        agentite_spatial_destroy(index);
    }

    SECTION("Create with minimum capacity") {
        Agentite_SpatialIndex *index = agentite_spatial_create(1);
        REQUIRE(index != nullptr);
        agentite_spatial_destroy(index);
    }

    SECTION("Create with zero capacity") {
        Agentite_SpatialIndex *index = agentite_spatial_create(0);
        REQUIRE(index != nullptr);
        agentite_spatial_destroy(index);
    }

    SECTION("Create with negative capacity") {
        Agentite_SpatialIndex *index = agentite_spatial_create(-10);
        REQUIRE(index != nullptr);
        agentite_spatial_destroy(index);
    }

    SECTION("Create with large capacity") {
        Agentite_SpatialIndex *index = agentite_spatial_create(10000);
        REQUIRE(index != nullptr);
        agentite_spatial_destroy(index);
    }

    SECTION("Destroy NULL is safe") {
        agentite_spatial_destroy(nullptr);
        // Should not crash
    }
}

/* ============================================================================
 * Basic Operations Tests
 * ============================================================================ */

TEST_CASE("Spatial index add operations", "[spatial][add]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    SECTION("Add single entity") {
        REQUIRE(agentite_spatial_add(index, 5, 10, 1));
        REQUIRE(agentite_spatial_has(index, 5, 10));
        REQUIRE(agentite_spatial_query(index, 5, 10) == 1);
    }

    SECTION("Add multiple entities to same cell") {
        for (uint32_t i = 1; i <= 5; i++) {
            REQUIRE(agentite_spatial_add(index, 0, 0, i));
        }
        REQUIRE(agentite_spatial_count_at(index, 0, 0) == 5);
    }

    SECTION("Add entities to different cells") {
        REQUIRE(agentite_spatial_add(index, 0, 0, 1));
        REQUIRE(agentite_spatial_add(index, 10, 10, 2));
        REQUIRE(agentite_spatial_add(index, -5, -5, 3));

        REQUIRE(agentite_spatial_query(index, 0, 0) == 1);
        REQUIRE(agentite_spatial_query(index, 10, 10) == 2);
        REQUIRE(agentite_spatial_query(index, -5, -5) == 3);
    }

    SECTION("Add with negative coordinates") {
        REQUIRE(agentite_spatial_add(index, -100, -200, 42));
        REQUIRE(agentite_spatial_has(index, -100, -200));
        REQUIRE(agentite_spatial_query(index, -100, -200) == 42);
    }

    SECTION("Add with extreme coordinates") {
        REQUIRE(agentite_spatial_add(index, INT32_MAX / 2, INT32_MIN / 2, 99));
        REQUIRE(agentite_spatial_has(index, INT32_MAX / 2, INT32_MIN / 2));
    }

    SECTION("Add invalid entity ID (0) fails") {
        REQUIRE_FALSE(agentite_spatial_add(index, 0, 0, AGENTITE_SPATIAL_INVALID));
    }

    SECTION("Add to NULL index fails") {
        REQUIRE_FALSE(agentite_spatial_add(nullptr, 0, 0, 1));
    }

    SECTION("Cell full rejection (MAX_PER_CELL)") {
        // Fill cell to max capacity
        for (uint32_t i = 1; i <= AGENTITE_SPATIAL_MAX_PER_CELL; i++) {
            REQUIRE(agentite_spatial_add(index, 0, 0, i));
        }

        // One more should fail
        REQUIRE_FALSE(agentite_spatial_add(index, 0, 0, AGENTITE_SPATIAL_MAX_PER_CELL + 1));
        REQUIRE(agentite_spatial_count_at(index, 0, 0) == AGENTITE_SPATIAL_MAX_PER_CELL);
    }

    agentite_spatial_destroy(index);
}

TEST_CASE("Spatial index remove operations", "[spatial][remove]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    SECTION("Remove single entity") {
        agentite_spatial_add(index, 5, 5, 1);
        REQUIRE(agentite_spatial_remove(index, 5, 5, 1));
        REQUIRE_FALSE(agentite_spatial_has(index, 5, 5));
    }

    SECTION("Remove from multiple entities in cell") {
        agentite_spatial_add(index, 0, 0, 1);
        agentite_spatial_add(index, 0, 0, 2);
        agentite_spatial_add(index, 0, 0, 3);

        REQUIRE(agentite_spatial_remove(index, 0, 0, 2));
        REQUIRE(agentite_spatial_count_at(index, 0, 0) == 2);
        REQUIRE(agentite_spatial_has_entity(index, 0, 0, 1));
        REQUIRE_FALSE(agentite_spatial_has_entity(index, 0, 0, 2));
        REQUIRE(agentite_spatial_has_entity(index, 0, 0, 3));
    }

    SECTION("Remove non-existent entity") {
        agentite_spatial_add(index, 0, 0, 1);
        REQUIRE_FALSE(agentite_spatial_remove(index, 0, 0, 999));
        REQUIRE(agentite_spatial_count_at(index, 0, 0) == 1);
    }

    SECTION("Remove from wrong position") {
        agentite_spatial_add(index, 0, 0, 1);
        REQUIRE_FALSE(agentite_spatial_remove(index, 1, 1, 1));
        REQUIRE(agentite_spatial_has(index, 0, 0));
    }

    SECTION("Remove from empty cell") {
        REQUIRE_FALSE(agentite_spatial_remove(index, 100, 100, 1));
    }

    SECTION("Remove from NULL index") {
        REQUIRE_FALSE(agentite_spatial_remove(nullptr, 0, 0, 1));
    }

    agentite_spatial_destroy(index);
}

TEST_CASE("Spatial index move operations", "[spatial][move]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    SECTION("Move entity to new position") {
        agentite_spatial_add(index, 0, 0, 1);
        REQUIRE(agentite_spatial_move(index, 0, 0, 10, 10, 1));

        REQUIRE_FALSE(agentite_spatial_has(index, 0, 0));
        REQUIRE(agentite_spatial_has(index, 10, 10));
        REQUIRE(agentite_spatial_query(index, 10, 10) == 1);
    }

    SECTION("Move entity to same position") {
        agentite_spatial_add(index, 5, 5, 1);
        REQUIRE(agentite_spatial_move(index, 5, 5, 5, 5, 1));
        REQUIRE(agentite_spatial_has(index, 5, 5));
        REQUIRE(agentite_spatial_count_at(index, 5, 5) == 1);
    }

    SECTION("Move from non-existent position still adds") {
        // Even if old position doesn't have entity, it adds to new position
        REQUIRE(agentite_spatial_move(index, 100, 100, 0, 0, 1));
        REQUIRE(agentite_spatial_has(index, 0, 0));
    }

    SECTION("Move with NULL index") {
        REQUIRE_FALSE(agentite_spatial_move(nullptr, 0, 0, 1, 1, 1));
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Query Operations Tests
 * ============================================================================ */

TEST_CASE("Spatial index query operations", "[spatial][query]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    // Set up test data
    agentite_spatial_add(index, 0, 0, 1);
    agentite_spatial_add(index, 0, 0, 2);
    agentite_spatial_add(index, 0, 0, 3);
    agentite_spatial_add(index, 5, 5, 10);

    SECTION("Query returns first entity") {
        uint32_t first = agentite_spatial_query(index, 0, 0);
        REQUIRE(first != AGENTITE_SPATIAL_INVALID);
        // First could be any of {1, 2, 3} depending on order
    }

    SECTION("Query empty cell returns invalid") {
        REQUIRE(agentite_spatial_query(index, 999, 999) == AGENTITE_SPATIAL_INVALID);
    }

    SECTION("Query all returns all entities") {
        uint32_t entities[10];
        int count = agentite_spatial_query_all(index, 0, 0, entities, 10);
        REQUIRE(count == 3);

        // Check all entities are present
        bool found[4] = {false, false, false, false};
        for (int i = 0; i < count; i++) {
            if (entities[i] >= 1 && entities[i] <= 3) {
                found[entities[i]] = true;
            }
        }
        REQUIRE(found[1]);
        REQUIRE(found[2]);
        REQUIRE(found[3]);
    }

    SECTION("Query all with limited buffer") {
        uint32_t entities[2];
        int count = agentite_spatial_query_all(index, 0, 0, entities, 2);
        REQUIRE(count == 2);  // Limited by buffer size
    }

    SECTION("Query all with NULL buffer") {
        int count = agentite_spatial_query_all(index, 0, 0, nullptr, 10);
        REQUIRE(count == 0);
    }

    SECTION("Count at position") {
        REQUIRE(agentite_spatial_count_at(index, 0, 0) == 3);
        REQUIRE(agentite_spatial_count_at(index, 5, 5) == 1);
        REQUIRE(agentite_spatial_count_at(index, 99, 99) == 0);
    }

    SECTION("Has entity specific") {
        REQUIRE(agentite_spatial_has_entity(index, 0, 0, 1));
        REQUIRE(agentite_spatial_has_entity(index, 0, 0, 2));
        REQUIRE(agentite_spatial_has_entity(index, 0, 0, 3));
        REQUIRE_FALSE(agentite_spatial_has_entity(index, 0, 0, 99));
        REQUIRE_FALSE(agentite_spatial_has_entity(index, 5, 5, 1));
    }

    SECTION("NULL index queries") {
        REQUIRE_FALSE(agentite_spatial_has(nullptr, 0, 0));
        REQUIRE(agentite_spatial_query(nullptr, 0, 0) == AGENTITE_SPATIAL_INVALID);
        REQUIRE(agentite_spatial_query_all(nullptr, 0, 0, nullptr, 0) == 0);
        REQUIRE(agentite_spatial_count_at(nullptr, 0, 0) == 0);
        REQUIRE_FALSE(agentite_spatial_has_entity(nullptr, 0, 0, 1));
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Region Query Tests
 * ============================================================================ */

TEST_CASE("Spatial index region queries", "[spatial][region]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    // Create a grid of entities
    // (-2,-2) to (2,2) with entity ID = (x+3)*10 + (y+3)
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            uint32_t id = (uint32_t)((x + 3) * 10 + (y + 3));
            agentite_spatial_add(index, x, y, id);
        }
    }

    SECTION("Query rect includes all cells") {
        Agentite_SpatialQueryResult results[100];
        int count = agentite_spatial_query_rect(index, -2, -2, 2, 2, results, 100);
        REQUIRE(count == 25);  // 5x5 grid
    }

    SECTION("Query rect with swapped coordinates") {
        Agentite_SpatialQueryResult results[100];
        // x1 > x2, y1 > y2 - should handle gracefully
        int count = agentite_spatial_query_rect(index, 2, 2, -2, -2, results, 100);
        REQUIRE(count == 25);  // Implementation should normalize
    }

    SECTION("Query rect single cell") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_rect(index, 0, 0, 0, 0, results, 10);
        REQUIRE(count == 1);
        REQUIRE(results[0].x == 0);
        REQUIRE(results[0].y == 0);
    }

    SECTION("Query rect partial overlap") {
        Agentite_SpatialQueryResult results[100];
        int count = agentite_spatial_query_rect(index, 0, 0, 5, 5, results, 100);
        // Should find (0,0) to (2,2) = 9 cells
        REQUIRE(count == 9);
    }

    SECTION("Query rect no overlap") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_rect(index, 100, 100, 200, 200, results, 10);
        REQUIRE(count == 0);
    }

    SECTION("Query rect with limited results buffer") {
        Agentite_SpatialQueryResult results[5];
        int count = agentite_spatial_query_rect(index, -2, -2, 2, 2, results, 5);
        REQUIRE(count == 5);  // Limited by buffer
    }

    SECTION("Query rect with NULL buffer") {
        int count = agentite_spatial_query_rect(index, -2, -2, 2, 2, nullptr, 100);
        REQUIRE(count == 0);
    }

    SECTION("Query rect with NULL index") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_rect(nullptr, 0, 0, 5, 5, results, 10);
        REQUIRE(count == 0);
    }

    agentite_spatial_destroy(index);
}

TEST_CASE("Spatial index radius queries", "[spatial][radius]") {
    // Use larger capacity to avoid hash collisions affecting results
    Agentite_SpatialIndex *index = agentite_spatial_create(256);
    REQUIRE(index != nullptr);

    // Create a grid of entities from (-5,-5) to (5,5) - 121 entities
    for (int x = -5; x <= 5; x++) {
        for (int y = -5; y <= 5; y++) {
            uint32_t id = (uint32_t)((x + 6) * 100 + (y + 6));
            agentite_spatial_add(index, x, y, id);
        }
    }

    // Verify all entities were added
    REQUIRE(agentite_spatial_total_count(index) == 121);

    SECTION("Query radius 0 (center only)") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_radius(index, 0, 0, 0, results, 10);
        REQUIRE(count == 1);
        REQUIRE(results[0].x == 0);
        REQUIRE(results[0].y == 0);
    }

    SECTION("Query radius 1 (3x3 Chebyshev)") {
        Agentite_SpatialQueryResult results[20];
        int count = agentite_spatial_query_radius(index, 0, 0, 1, results, 20);
        // Should be 9 entities in the 3x3 square from (-1,-1) to (1,1)
        REQUIRE(count == 9);
    }

    SECTION("Query radius 2 (5x5 Chebyshev)") {
        Agentite_SpatialQueryResult results[50];
        int count = agentite_spatial_query_radius(index, 0, 0, 2, results, 50);
        // Should be 25 entities in the 5x5 square from (-2,-2) to (2,2)
        REQUIRE(count == 25);
    }

    SECTION("Query radius with offset center") {
        Agentite_SpatialQueryResult results[20];
        int count = agentite_spatial_query_radius(index, 3, 3, 1, results, 20);
        // Should be 9 entities: (2,2) to (4,4)
        REQUIRE(count == 9);
    }

    SECTION("Query radius at edge") {
        Agentite_SpatialQueryResult results[20];
        // Centered at (5,5), radius 1 - only partial coverage
        int count = agentite_spatial_query_radius(index, 5, 5, 1, results, 20);
        // Only 4 cells in our grid: (4,4), (4,5), (5,4), (5,5)
        REQUIRE(count == 4);
    }

    SECTION("Query radius NULL index") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_radius(nullptr, 0, 0, 2, results, 10);
        REQUIRE(count == 0);
    }

    agentite_spatial_destroy(index);
}

TEST_CASE("Spatial index circle queries", "[spatial][circle]") {
    // Use larger capacity to avoid hash collision issues
    Agentite_SpatialIndex *index = agentite_spatial_create(256);
    REQUIRE(index != nullptr);

    // Create a grid of entities from (-5,-5) to (5,5)
    for (int x = -5; x <= 5; x++) {
        for (int y = -5; y <= 5; y++) {
            uint32_t id = (uint32_t)((x + 6) * 100 + (y + 6));
            agentite_spatial_add(index, x, y, id);
        }
    }

    // Verify all entities were added
    REQUIRE(agentite_spatial_total_count(index) == 121);

    SECTION("Query circle radius 0 (center only)") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_circle(index, 0, 0, 0, results, 10);
        REQUIRE(count == 1);
    }

    SECTION("Query circle radius 1 (center + 4 cardinal directions)") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_circle(index, 0, 0, 1, results, 10);
        // Euclidean distance <= 1: center (0,0) and 4 cardinals
        // Corners (1,1), etc have distance sqrt(2) > 1
        REQUIRE(count == 5);
    }

    SECTION("Query circle radius 2") {
        Agentite_SpatialQueryResult results[30];
        int count = agentite_spatial_query_circle(index, 0, 0, 2, results, 30);
        // Euclidean distance <= 2 includes:
        // - center (0,0)
        // - cardinals at distance 1: (1,0), (0,1), (-1,0), (0,-1)
        // - diagonals at sqrt(2) ~= 1.41: (1,1), (1,-1), (-1,1), (-1,-1)
        // - cardinals at distance 2: (2,0), (0,2), (-2,0), (0,-2)
        // Total = 1 + 4 + 4 + 4 = 13
        REQUIRE(count == 13);
    }

    SECTION("Query circle NULL index") {
        Agentite_SpatialQueryResult results[10];
        int count = agentite_spatial_query_circle(nullptr, 0, 0, 2, results, 10);
        REQUIRE(count == 0);
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Iterator Tests
 * ============================================================================ */

TEST_CASE("Spatial index iteration", "[spatial][iterator]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    SECTION("Iterate empty cell") {
        Agentite_SpatialIterator iter = agentite_spatial_iter_begin(index, 0, 0);
        REQUIRE_FALSE(agentite_spatial_iter_valid(&iter));
    }

    SECTION("Iterate single entity") {
        agentite_spatial_add(index, 0, 0, 42);

        Agentite_SpatialIterator iter = agentite_spatial_iter_begin(index, 0, 0);
        REQUIRE(agentite_spatial_iter_valid(&iter));
        REQUIRE(agentite_spatial_iter_get(&iter) == 42);

        agentite_spatial_iter_next(&iter);
        REQUIRE_FALSE(agentite_spatial_iter_valid(&iter));
    }

    SECTION("Iterate multiple entities") {
        agentite_spatial_add(index, 0, 0, 1);
        agentite_spatial_add(index, 0, 0, 2);
        agentite_spatial_add(index, 0, 0, 3);

        std::vector<uint32_t> found;
        Agentite_SpatialIterator iter = agentite_spatial_iter_begin(index, 0, 0);
        while (agentite_spatial_iter_valid(&iter)) {
            found.push_back(agentite_spatial_iter_get(&iter));
            agentite_spatial_iter_next(&iter);
        }

        REQUIRE(found.size() == 3);
    }

    SECTION("Iterator NULL checks") {
        REQUIRE_FALSE(agentite_spatial_iter_valid(nullptr));
        REQUIRE(agentite_spatial_iter_get(nullptr) == AGENTITE_SPATIAL_INVALID);
        agentite_spatial_iter_next(nullptr);  // Should not crash
    }

    SECTION("Iterator with NULL index") {
        Agentite_SpatialIterator iter = agentite_spatial_iter_begin(nullptr, 0, 0);
        REQUIRE_FALSE(agentite_spatial_iter_valid(&iter));
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_CASE("Spatial index statistics", "[spatial][stats]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    SECTION("Empty index stats") {
        REQUIRE(agentite_spatial_total_count(index) == 0);
        REQUIRE(agentite_spatial_occupied_cells(index) == 0);
        REQUIRE(agentite_spatial_load_factor(index) == 0.0f);
    }

    SECTION("Stats after adding entities") {
        agentite_spatial_add(index, 0, 0, 1);
        agentite_spatial_add(index, 0, 0, 2);
        agentite_spatial_add(index, 1, 1, 3);

        REQUIRE(agentite_spatial_total_count(index) == 3);
        REQUIRE(agentite_spatial_occupied_cells(index) == 2);
        REQUIRE(agentite_spatial_load_factor(index) > 0.0f);
    }

    SECTION("Stats after removing entities") {
        agentite_spatial_add(index, 0, 0, 1);
        agentite_spatial_add(index, 0, 0, 2);
        agentite_spatial_remove(index, 0, 0, 1);

        REQUIRE(agentite_spatial_total_count(index) == 1);
        REQUIRE(agentite_spatial_occupied_cells(index) == 1);
    }

    SECTION("Stats NULL index") {
        REQUIRE(agentite_spatial_total_count(nullptr) == 0);
        REQUIRE(agentite_spatial_occupied_cells(nullptr) == 0);
        REQUIRE(agentite_spatial_load_factor(nullptr) == 0.0f);
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Clear Tests
 * ============================================================================ */

TEST_CASE("Spatial index clear", "[spatial][clear]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    SECTION("Clear populated index") {
        for (int i = 0; i < 100; i++) {
            agentite_spatial_add(index, i, i, (uint32_t)(i + 1));
        }
        REQUIRE(agentite_spatial_total_count(index) == 100);

        agentite_spatial_clear(index);

        REQUIRE(agentite_spatial_total_count(index) == 0);
        REQUIRE(agentite_spatial_occupied_cells(index) == 0);
        REQUIRE_FALSE(agentite_spatial_has(index, 0, 0));
    }

    SECTION("Clear empty index") {
        agentite_spatial_clear(index);
        REQUIRE(agentite_spatial_total_count(index) == 0);
    }

    SECTION("Clear NULL index is safe") {
        agentite_spatial_clear(nullptr);
        // Should not crash
    }

    SECTION("Can add after clear") {
        agentite_spatial_add(index, 0, 0, 1);
        agentite_spatial_clear(index);
        REQUIRE(agentite_spatial_add(index, 0, 0, 2));
        REQUIRE(agentite_spatial_query(index, 0, 0) == 2);
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Hash Collision Tests
 * ============================================================================ */

TEST_CASE("Spatial index hash collisions", "[spatial][collision]") {
    // Use small capacity to force collisions
    Agentite_SpatialIndex *index = agentite_spatial_create(4);
    REQUIRE(index != nullptr);

    SECTION("Add many entities forces collisions") {
        // Add more cells than capacity to force collision handling
        for (int i = 0; i < 50; i++) {
            REQUIRE(agentite_spatial_add(index, i * 7, i * 11, (uint32_t)(i + 1)));
        }

        // Verify all are retrievable
        for (int i = 0; i < 50; i++) {
            REQUIRE(agentite_spatial_has(index, i * 7, i * 11));
            REQUIRE(agentite_spatial_query(index, i * 7, i * 11) == (uint32_t)(i + 1));
        }
    }

    SECTION("Collisions with negative coordinates") {
        // This test uses the small capacity (4) index created for collision testing
        // The hash table will grow as needed to handle these entries
        for (int i = -5; i <= 5; i++) {
            REQUIRE(agentite_spatial_add(index, i, -i, (uint32_t)(i + 6)));
        }

        // Verify all are retrievable after possible rehashing
        for (int i = -5; i <= 5; i++) {
            REQUIRE(agentite_spatial_has(index, i, -i));
        }
    }

    SECTION("Remove with collisions") {
        // Add then remove to test collision chain integrity
        for (int i = 0; i < 30; i++) {
            agentite_spatial_add(index, i, i, (uint32_t)(i + 1));
        }

        // Remove every other one
        for (int i = 0; i < 30; i += 2) {
            REQUIRE(agentite_spatial_remove(index, i, i, (uint32_t)(i + 1)));
        }

        // Verify remaining are still accessible
        for (int i = 1; i < 30; i += 2) {
            REQUIRE(agentite_spatial_has(index, i, i));
        }
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Rehashing / Growth Tests
 * ============================================================================ */

TEST_CASE("Spatial index rehashing/growth", "[spatial][rehash]") {
    // Start with minimal capacity
    Agentite_SpatialIndex *index = agentite_spatial_create(2);
    REQUIRE(index != nullptr);

    SECTION("Index grows with many insertions") {
        // Add many more entities than initial capacity
        int count = 500;
        for (int i = 0; i < count; i++) {
            REQUIRE(agentite_spatial_add(index, i, i * 2, (uint32_t)(i + 1)));
        }

        REQUIRE(agentite_spatial_total_count(index) == count);

        // All should still be accessible after growth
        for (int i = 0; i < count; i++) {
            REQUIRE(agentite_spatial_has(index, i, i * 2));
            REQUIRE(agentite_spatial_query(index, i, i * 2) == (uint32_t)(i + 1));
        }

        // Load factor should be reasonable after growth
        float lf = agentite_spatial_load_factor(index);
        REQUIRE(lf < 0.8f);  // Should have grown to maintain good load factor
    }

    SECTION("Operations work during high load") {
        // Fill to high load factor
        for (int i = 0; i < 100; i++) {
            agentite_spatial_add(index, i, 0, (uint32_t)(i + 1));
        }

        // Perform mixed operations
        REQUIRE(agentite_spatial_remove(index, 50, 0, 51));
        REQUIRE(agentite_spatial_move(index, 25, 0, 1000, 1000, 26));
        REQUIRE(agentite_spatial_add(index, 200, 200, 999));

        // Verify consistency
        REQUIRE_FALSE(agentite_spatial_has(index, 50, 0));
        REQUIRE_FALSE(agentite_spatial_has(index, 25, 0));
        REQUIRE(agentite_spatial_has(index, 1000, 1000));
        REQUIRE(agentite_spatial_has(index, 200, 200));
    }

    agentite_spatial_destroy(index);
}

/* ============================================================================
 * Edge Cases and Stress Tests
 * ============================================================================ */

TEST_CASE("Spatial index edge cases", "[spatial][edge]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(64);
    REQUIRE(index != nullptr);

    SECTION("Duplicate entity in same cell") {
        // Adding same entity twice to same cell stores it twice
        REQUIRE(agentite_spatial_add(index, 0, 0, 1));
        REQUIRE(agentite_spatial_add(index, 0, 0, 1));
        REQUIRE(agentite_spatial_count_at(index, 0, 0) == 2);

        // Removing once removes one instance
        REQUIRE(agentite_spatial_remove(index, 0, 0, 1));
        REQUIRE(agentite_spatial_count_at(index, 0, 0) == 1);
        REQUIRE(agentite_spatial_has(index, 0, 0));
    }

    SECTION("Large coordinate values") {
        int large = 1000000;
        REQUIRE(agentite_spatial_add(index, large, large, 1));
        REQUIRE(agentite_spatial_add(index, -large, -large, 2));
        REQUIRE(agentite_spatial_has(index, large, large));
        REQUIRE(agentite_spatial_has(index, -large, -large));
    }

    SECTION("Max entity ID") {
        uint32_t max_id = UINT32_MAX;
        REQUIRE(agentite_spatial_add(index, 0, 0, max_id));
        REQUIRE(agentite_spatial_query(index, 0, 0) == max_id);
    }

    SECTION("Rapid add/remove cycles") {
        for (int cycle = 0; cycle < 100; cycle++) {
            REQUIRE(agentite_spatial_add(index, 0, 0, 1));
            REQUIRE(agentite_spatial_remove(index, 0, 0, 1));
        }
        REQUIRE(agentite_spatial_total_count(index) == 0);
    }

    SECTION("Query with zero max_results") {
        agentite_spatial_add(index, 0, 0, 1);
        uint32_t entities[1];
        int count = agentite_spatial_query_all(index, 0, 0, entities, 0);
        REQUIRE(count == 0);
    }

    agentite_spatial_destroy(index);
}

TEST_CASE("Spatial index stress test", "[spatial][stress]") {
    Agentite_SpatialIndex *index = agentite_spatial_create(16);
    REQUIRE(index != nullptr);

    SECTION("Many random operations") {
        // Perform many mixed operations
        for (int i = 0; i < 1000; i++) {
            int x = (i * 17) % 100 - 50;
            int y = (i * 31) % 100 - 50;
            uint32_t id = (uint32_t)((i % 500) + 1);

            if (i % 3 == 0) {
                agentite_spatial_add(index, x, y, id);
            } else if (i % 3 == 1) {
                agentite_spatial_remove(index, x, y, id);
            } else {
                agentite_spatial_move(index, x, y, y, x, id);
            }
        }

        // Should still be operational
        REQUIRE(agentite_spatial_add(index, 0, 0, 9999));
        REQUIRE(agentite_spatial_has(index, 0, 0));
    }

    SECTION("Many region queries") {
        // Populate
        for (int x = 0; x < 20; x++) {
            for (int y = 0; y < 20; y++) {
                agentite_spatial_add(index, x, y, (uint32_t)(x * 20 + y + 1));
            }
        }

        // Perform many queries
        Agentite_SpatialQueryResult results[100];
        for (int i = 0; i < 100; i++) {
            int x1 = i % 10;
            int y1 = i % 10;
            agentite_spatial_query_rect(index, x1, y1, x1 + 5, y1 + 5, results, 100);
        }

        // Should still work correctly
        REQUIRE(agentite_spatial_total_count(index) == 400);
    }

    agentite_spatial_destroy(index);
}
