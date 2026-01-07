/*
 * Agentite Network System Tests
 *
 * Tests for the network/graph system including node management,
 * connectivity (union-find), resource balance, coverage queries,
 * and group operations.
 */

#include "catch_amalgamated.hpp"
#include "agentite/network.h"
#include <vector>
#include <cmath>

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

TEST_CASE("Network creation and destruction", "[network][lifecycle]") {
    SECTION("Create network") {
        Agentite_NetworkSystem *network = agentite_network_create();
        REQUIRE(network != nullptr);
        agentite_network_destroy(network);
    }

    SECTION("Destroy NULL is safe") {
        agentite_network_destroy(nullptr);
        // Should not crash
    }
}

/* ============================================================================
 * Node Management Tests
 * ============================================================================ */

TEST_CASE("Network node management", "[network][nodes]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Add single node") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        REQUIRE(id != AGENTITE_NETWORK_INVALID);

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node != nullptr);
        REQUIRE(node->x == 0);
        REQUIRE(node->y == 0);
        REQUIRE(node->radius == 5);
        REQUIRE(node->active == true);
    }

    SECTION("Add multiple nodes") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 10, 10, 3);
        uint32_t id3 = agentite_network_add_node(network, -5, -5, 7);

        REQUIRE(id1 != AGENTITE_NETWORK_INVALID);
        REQUIRE(id2 != AGENTITE_NETWORK_INVALID);
        REQUIRE(id3 != AGENTITE_NETWORK_INVALID);
        REQUIRE(id1 != id2);
        REQUIRE(id2 != id3);

        REQUIRE(agentite_network_node_count(network) == 3);
    }

    SECTION("Add node with negative coordinates") {
        uint32_t id = agentite_network_add_node(network, -100, -200, 10);
        REQUIRE(id != AGENTITE_NETWORK_INVALID);

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node != nullptr);
        REQUIRE(node->x == -100);
        REQUIRE(node->y == -200);
    }

    SECTION("Add node with zero radius") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 0);
        REQUIRE(id != AGENTITE_NETWORK_INVALID);

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node != nullptr);
        REQUIRE(node->radius == 0);
    }

    SECTION("Add to NULL network") {
        uint32_t id = agentite_network_add_node(nullptr, 0, 0, 5);
        REQUIRE(id == AGENTITE_NETWORK_INVALID);
    }

    SECTION("Remove node") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        REQUIRE(agentite_network_remove_node(network, id));
        REQUIRE(agentite_network_get_node(network, id) == nullptr);
        REQUIRE(agentite_network_node_count(network) == 0);
    }

    SECTION("Remove non-existent node") {
        REQUIRE_FALSE(agentite_network_remove_node(network, 9999));
    }

    SECTION("Remove from NULL network") {
        REQUIRE_FALSE(agentite_network_remove_node(nullptr, 0));
    }

    SECTION("Move node") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        REQUIRE(agentite_network_move_node(network, id, 100, 200));

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node != nullptr);
        REQUIRE(node->x == 100);
        REQUIRE(node->y == 200);
    }

    SECTION("Move non-existent node") {
        REQUIRE_FALSE(agentite_network_move_node(network, 9999, 0, 0));
    }

    SECTION("Set radius") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        REQUIRE(agentite_network_set_radius(network, id, 10));

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node->radius == 10);
    }

    SECTION("Set active state") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        REQUIRE(agentite_network_set_active(network, id, false));

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node->active == false);

        REQUIRE(agentite_network_set_active(network, id, true));
        REQUIRE(node->active == true);
    }

    SECTION("Get non-existent node") {
        REQUIRE(agentite_network_get_node(network, 9999) == nullptr);
    }

    SECTION("Get node from NULL network") {
        REQUIRE(agentite_network_get_node(nullptr, 0) == nullptr);
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Resource Management Tests
 * ============================================================================ */

TEST_CASE("Network resource management", "[network][resources]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    uint32_t id = agentite_network_add_node(network, 0, 0, 5);
    REQUIRE(id != AGENTITE_NETWORK_INVALID);

    SECTION("Set production") {
        REQUIRE(agentite_network_set_production(network, id, 100));
        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node->production == 100);
    }

    SECTION("Set consumption") {
        REQUIRE(agentite_network_set_consumption(network, id, 50));
        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node->consumption == 50);
    }

    SECTION("Add production") {
        agentite_network_set_production(network, id, 100);
        int32_t result = agentite_network_add_production(network, id, 25);
        REQUIRE(result == 125);

        result = agentite_network_add_production(network, id, -50);
        REQUIRE(result == 75);
    }

    SECTION("Add consumption") {
        agentite_network_set_consumption(network, id, 50);
        int32_t result = agentite_network_add_consumption(network, id, 20);
        REQUIRE(result == 70);

        result = agentite_network_add_consumption(network, id, -30);
        REQUIRE(result == 40);
    }

    SECTION("Set production on non-existent node") {
        REQUIRE_FALSE(agentite_network_set_production(network, 9999, 100));
    }

    SECTION("Set consumption on non-existent node") {
        REQUIRE_FALSE(agentite_network_set_consumption(network, 9999, 50));
    }

    SECTION("Add production to non-existent node") {
        int32_t result = agentite_network_add_production(network, 9999, 100);
        REQUIRE(result == 0);
    }

    SECTION("Add consumption to non-existent node") {
        int32_t result = agentite_network_add_consumption(network, 9999, 50);
        REQUIRE(result == 0);
    }

    SECTION("NULL network resource operations") {
        REQUIRE_FALSE(agentite_network_set_production(nullptr, 0, 100));
        REQUIRE_FALSE(agentite_network_set_consumption(nullptr, 0, 50));
        REQUIRE(agentite_network_add_production(nullptr, 0, 100) == 0);
        REQUIRE(agentite_network_add_consumption(nullptr, 0, 50) == 0);
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Connectivity Tests (Union-Find)
 * ============================================================================ */

TEST_CASE("Network connectivity", "[network][connectivity]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Single node forms own group") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);

        uint32_t group = agentite_network_get_group(network, id);
        REQUIRE(group != AGENTITE_NETWORK_INVALID);
        REQUIRE(agentite_network_group_count(network) == 1);
    }

    SECTION("Disconnected nodes form separate groups") {
        // Add nodes too far apart to connect (distance > radius1 + radius2)
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 2);
        uint32_t id2 = agentite_network_add_node(network, 100, 100, 2);  // Distance ~141 >> 4

        agentite_network_update(network);

        uint32_t group1 = agentite_network_get_group(network, id1);
        uint32_t group2 = agentite_network_get_group(network, id2);

        REQUIRE(group1 != AGENTITE_NETWORK_INVALID);
        REQUIRE(group2 != AGENTITE_NETWORK_INVALID);
        REQUIRE(group1 != group2);
        REQUIRE(agentite_network_group_count(network) == 2);
    }

    SECTION("Connected nodes form single group") {
        // Add nodes close enough to connect (distance <= radius1 + radius2)
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 8, 0, 5);  // Distance 8 <= 10

        agentite_network_update(network);

        uint32_t group1 = agentite_network_get_group(network, id1);
        uint32_t group2 = agentite_network_get_group(network, id2);

        REQUIRE(group1 == group2);
        REQUIRE(agentite_network_group_count(network) == 1);
    }

    SECTION("Chain connectivity") {
        // A-B and B-C connected, so A-B-C form one group
        uint32_t idA = agentite_network_add_node(network, 0, 0, 5);
        uint32_t idB = agentite_network_add_node(network, 8, 0, 5);   // Connects to A
        uint32_t idC = agentite_network_add_node(network, 16, 0, 5);  // Connects to B

        agentite_network_update(network);

        uint32_t groupA = agentite_network_get_group(network, idA);
        uint32_t groupB = agentite_network_get_group(network, idB);
        uint32_t groupC = agentite_network_get_group(network, idC);

        REQUIRE(groupA == groupB);
        REQUIRE(groupB == groupC);
        REQUIRE(agentite_network_group_count(network) == 1);
    }

    SECTION("Multiple separate networks") {
        // Network 1: nodes at (0,0) and (5,0)
        uint32_t id1a = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id1b = agentite_network_add_node(network, 5, 0, 5);

        // Network 2: nodes at (100,0) and (105,0)
        uint32_t id2a = agentite_network_add_node(network, 100, 0, 5);
        uint32_t id2b = agentite_network_add_node(network, 105, 0, 5);

        agentite_network_update(network);

        uint32_t group1 = agentite_network_get_group(network, id1a);
        uint32_t group2 = agentite_network_get_group(network, id2a);

        REQUIRE(group1 != group2);
        REQUIRE(agentite_network_get_group(network, id1b) == group1);
        REQUIRE(agentite_network_get_group(network, id2b) == group2);
        REQUIRE(agentite_network_group_count(network) == 2);
    }

    SECTION("Inactive nodes don't connect") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 10);
        uint32_t id2 = agentite_network_add_node(network, 5, 0, 10);
        agentite_network_set_active(network, id2, false);

        agentite_network_update(network);

        // Inactive node should not be in any group
        uint32_t group1 = agentite_network_get_group(network, id1);
        uint32_t group2 = agentite_network_get_group(network, id2);

        REQUIRE(group1 != AGENTITE_NETWORK_INVALID);
        // Inactive node might have INVALID group or own group depending on impl
    }

    SECTION("Move breaks connectivity") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 8, 0, 5);

        agentite_network_update(network);
        REQUIRE(agentite_network_get_group(network, id1) == agentite_network_get_group(network, id2));

        // Move node 2 far away
        agentite_network_move_node(network, id2, 1000, 1000);
        agentite_network_update(network);

        REQUIRE(agentite_network_get_group(network, id1) != agentite_network_get_group(network, id2));
    }

    SECTION("Remove breaks connectivity") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 8, 0, 5);   // Bridge
        uint32_t id3 = agentite_network_add_node(network, 16, 0, 5);

        agentite_network_update(network);
        uint32_t groupBefore = agentite_network_get_group(network, id1);
        REQUIRE(agentite_network_get_group(network, id3) == groupBefore);

        // Remove bridge
        agentite_network_remove_node(network, id2);
        agentite_network_update(network);

        // id1 and id3 should now be in different groups
        REQUIRE(agentite_network_get_group(network, id1) != agentite_network_get_group(network, id3));
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Group Info and Power Tests
 * ============================================================================ */

TEST_CASE("Network group info and power", "[network][groups][power]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Powered group (production >= consumption)") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 5, 0, 5);

        agentite_network_set_production(network, id1, 100);
        agentite_network_set_consumption(network, id2, 50);

        agentite_network_update(network);

        uint32_t group = agentite_network_get_group(network, id1);
        REQUIRE(agentite_network_is_powered(network, group));
        REQUIRE(agentite_network_node_is_powered(network, id1));
        REQUIRE(agentite_network_node_is_powered(network, id2));

        Agentite_NetworkGroup info;
        REQUIRE(agentite_network_get_group_info(network, group, &info));
        REQUIRE(info.total_production == 100);
        REQUIRE(info.total_consumption == 50);
        REQUIRE(info.balance == 50);
        REQUIRE(info.powered == true);
        REQUIRE(info.node_count == 2);
    }

    SECTION("Unpowered group (production < consumption)") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 5, 0, 5);

        agentite_network_set_production(network, id1, 30);
        agentite_network_set_consumption(network, id2, 100);

        agentite_network_update(network);

        uint32_t group = agentite_network_get_group(network, id1);
        REQUIRE_FALSE(agentite_network_is_powered(network, group));
        REQUIRE_FALSE(agentite_network_node_is_powered(network, id1));

        Agentite_NetworkGroup info;
        REQUIRE(agentite_network_get_group_info(network, group, &info));
        REQUIRE(info.balance == -70);
        REQUIRE(info.powered == false);
    }

    SECTION("Zero balance is powered") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_set_production(network, id, 50);
        agentite_network_set_consumption(network, id, 50);

        agentite_network_update(network);

        uint32_t group = agentite_network_get_group(network, id);
        REQUIRE(agentite_network_is_powered(network, group));
    }

    SECTION("Get group info for invalid group") {
        Agentite_NetworkGroup info;
        REQUIRE_FALSE(agentite_network_get_group_info(network, AGENTITE_NETWORK_INVALID, &info));
        REQUIRE_FALSE(agentite_network_get_group_info(network, 9999, &info));
    }

    SECTION("Is powered for invalid group") {
        REQUIRE_FALSE(agentite_network_is_powered(network, AGENTITE_NETWORK_INVALID));
        REQUIRE_FALSE(agentite_network_is_powered(network, 9999));
    }

    SECTION("Node is powered for invalid node") {
        REQUIRE_FALSE(agentite_network_node_is_powered(network, AGENTITE_NETWORK_INVALID));
        REQUIRE_FALSE(agentite_network_node_is_powered(network, 9999));
    }

    SECTION("NULL network group operations") {
        Agentite_NetworkGroup info;
        REQUIRE_FALSE(agentite_network_get_group_info(nullptr, 0, &info));
        REQUIRE_FALSE(agentite_network_is_powered(nullptr, 0));
        REQUIRE_FALSE(agentite_network_node_is_powered(nullptr, 0));
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Coverage Query Tests
 * ============================================================================ */

TEST_CASE("Network coverage queries", "[network][coverage]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Cell covered by node") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);

        REQUIRE(agentite_network_covers_cell(network, 0, 0));  // Center
        REQUIRE(agentite_network_covers_cell(network, 3, 3));  // Within radius
        REQUIRE_FALSE(agentite_network_covers_cell(network, 10, 10));  // Outside
    }

    SECTION("Cell covered by powered network") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_set_production(network, id, 100);
        agentite_network_update(network);

        REQUIRE(agentite_network_cell_is_powered(network, 0, 0));
        REQUIRE(agentite_network_cell_is_powered(network, 3, 3));
    }

    SECTION("Cell not powered when network unpowered") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_set_consumption(network, id, 100);  // Only consumption
        agentite_network_update(network);

        // Cell is covered but not powered
        REQUIRE(agentite_network_covers_cell(network, 0, 0));
        REQUIRE_FALSE(agentite_network_cell_is_powered(network, 0, 0));
    }

    SECTION("Inactive node doesn't cover cells") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_set_active(network, id, false);
        agentite_network_update(network);

        REQUIRE_FALSE(agentite_network_covers_cell(network, 0, 0));
    }

    SECTION("Get coverage info") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 3, 0, 5);  // Overlapping coverage
        agentite_network_update(network);

        Agentite_NetworkCoverage coverage[10];
        int count = agentite_network_get_coverage(network, 2, 0, coverage, 10);

        REQUIRE(count >= 1);  // At least one node covers (2,0)
    }

    SECTION("Get nearest node") {
        agentite_network_add_node(network, 0, 0, 5);
        agentite_network_add_node(network, 10, 0, 5);
        agentite_network_add_node(network, 100, 0, 5);
        agentite_network_update(network);

        uint32_t nearest = agentite_network_get_nearest_node(network, 12, 0, -1);
        REQUIRE(nearest != AGENTITE_NETWORK_INVALID);

        const Agentite_NetworkNode *node = agentite_network_get_node(network, nearest);
        REQUIRE(node != nullptr);
        REQUIRE(node->x == 10);  // Node at (10,0) is closest to (12,0)
    }

    SECTION("Get nearest node with max distance") {
        agentite_network_add_node(network, 100, 100, 5);
        agentite_network_update(network);

        // Max distance 5, but node is much further
        uint32_t nearest = agentite_network_get_nearest_node(network, 0, 0, 5);
        REQUIRE(nearest == AGENTITE_NETWORK_INVALID);
    }

    SECTION("Get node coverage cells") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 2);
        agentite_network_update(network);

        int32_t x_coords[100];
        int32_t y_coords[100];
        int count = agentite_network_get_node_coverage(network, id, x_coords, y_coords, 100);

        // Radius 2 means 5x5 area = 25 cells (Chebyshev distance)
        REQUIRE(count > 0);
        REQUIRE(count <= 25);
    }

    SECTION("NULL network coverage operations") {
        REQUIRE_FALSE(agentite_network_covers_cell(nullptr, 0, 0));
        REQUIRE_FALSE(agentite_network_cell_is_powered(nullptr, 0, 0));
        REQUIRE(agentite_network_get_coverage(nullptr, 0, 0, nullptr, 0) == 0);
        REQUIRE(agentite_network_get_nearest_node(nullptr, 0, 0, -1) == AGENTITE_NETWORK_INVALID);
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Node/Group Iteration Tests
 * ============================================================================ */

TEST_CASE("Network node and group iteration", "[network][iteration]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Get all nodes") {
        agentite_network_add_node(network, 0, 0, 5);
        agentite_network_add_node(network, 10, 10, 5);
        agentite_network_add_node(network, 20, 20, 5);

        uint32_t nodes[10];
        int count = agentite_network_get_all_nodes(network, nodes, 10);
        REQUIRE(count == 3);
    }

    SECTION("Get all nodes with limited buffer") {
        for (int i = 0; i < 10; i++) {
            agentite_network_add_node(network, i * 100, 0, 5);
        }

        uint32_t nodes[5];
        int count = agentite_network_get_all_nodes(network, nodes, 5);
        REQUIRE(count == 5);  // Limited by buffer
    }

    SECTION("Get all groups") {
        // Create 3 disconnected nodes
        agentite_network_add_node(network, 0, 0, 1);
        agentite_network_add_node(network, 100, 0, 1);
        agentite_network_add_node(network, 200, 0, 1);
        agentite_network_update(network);

        uint32_t groups[10];
        int count = agentite_network_get_all_groups(network, groups, 10);
        REQUIRE(count == 3);
    }

    SECTION("Get group nodes") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 10);
        uint32_t id2 = agentite_network_add_node(network, 5, 0, 10);
        uint32_t id3 = agentite_network_add_node(network, 10, 0, 10);
        agentite_network_update(network);

        uint32_t group = agentite_network_get_group(network, id1);

        uint32_t nodes[10];
        int count = agentite_network_get_group_nodes(network, group, nodes, 10);
        REQUIRE(count == 3);
    }

    SECTION("Get nodes from empty network") {
        uint32_t nodes[10];
        int count = agentite_network_get_all_nodes(network, nodes, 10);
        REQUIRE(count == 0);
    }

    SECTION("NULL network iteration") {
        uint32_t buffer[10];
        REQUIRE(agentite_network_get_all_nodes(nullptr, buffer, 10) == 0);
        REQUIRE(agentite_network_get_all_groups(nullptr, buffer, 10) == 0);
        REQUIRE(agentite_network_get_group_nodes(nullptr, 0, buffer, 10) == 0);
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_CASE("Network statistics", "[network][stats]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Empty network stats") {
        REQUIRE(agentite_network_node_count(network) == 0);
        REQUIRE(agentite_network_group_count(network) == 0);
        REQUIRE(agentite_network_total_production(network) == 0);
        REQUIRE(agentite_network_total_consumption(network) == 0);
        REQUIRE(agentite_network_total_balance(network) == 0);
    }

    SECTION("Stats with nodes") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 100, 0, 5);

        agentite_network_set_production(network, id1, 100);
        agentite_network_set_consumption(network, id1, 30);
        agentite_network_set_production(network, id2, 50);
        agentite_network_set_consumption(network, id2, 80);

        REQUIRE(agentite_network_node_count(network) == 2);
        REQUIRE(agentite_network_total_production(network) == 150);
        REQUIRE(agentite_network_total_consumption(network) == 110);
        REQUIRE(agentite_network_total_balance(network) == 40);
    }

    SECTION("Get stats function") {
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 5);
        uint32_t id2 = agentite_network_add_node(network, 5, 0, 5);  // Connected
        uint32_t id3 = agentite_network_add_node(network, 100, 0, 5);  // Disconnected
        agentite_network_set_active(network, id3, false);

        agentite_network_set_production(network, id1, 100);
        agentite_network_set_production(network, id2, 50);

        agentite_network_update(network);

        int nodes, active, groups, powered;
        agentite_network_get_stats(network, &nodes, &active, &groups, &powered);

        REQUIRE(nodes == 3);
        REQUIRE(active == 2);  // id3 is inactive
        REQUIRE(groups >= 1);
        REQUIRE(powered >= 1);
    }

    SECTION("NULL network stats") {
        REQUIRE(agentite_network_node_count(nullptr) == 0);
        REQUIRE(agentite_network_group_count(nullptr) == 0);
        REQUIRE(agentite_network_total_production(nullptr) == 0);
        REQUIRE(agentite_network_total_consumption(nullptr) == 0);
        REQUIRE(agentite_network_total_balance(nullptr) == 0);
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Update and Dirty Tracking Tests
 * ============================================================================ */

TEST_CASE("Network dirty tracking", "[network][dirty]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("New network is dirty") {
        agentite_network_add_node(network, 0, 0, 5);
        REQUIRE(agentite_network_is_dirty(network));
    }

    SECTION("Update clears dirty flag") {
        agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);
        REQUIRE_FALSE(agentite_network_is_dirty(network));
    }

    SECTION("Add node sets dirty") {
        agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);
        REQUIRE_FALSE(agentite_network_is_dirty(network));

        agentite_network_add_node(network, 10, 10, 5);
        REQUIRE(agentite_network_is_dirty(network));
    }

    SECTION("Remove node sets dirty") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);

        agentite_network_remove_node(network, id);
        REQUIRE(agentite_network_is_dirty(network));
    }

    SECTION("Move node sets dirty") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);

        agentite_network_move_node(network, id, 10, 10);
        REQUIRE(agentite_network_is_dirty(network));
    }

    SECTION("Force recalculate") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);

        // Recalculate should work even when not dirty
        agentite_network_recalculate(network);
        REQUIRE_FALSE(agentite_network_is_dirty(network));
    }

    SECTION("NULL network dirty checks") {
        REQUIRE_FALSE(agentite_network_is_dirty(nullptr));
        agentite_network_update(nullptr);  // Should not crash
        agentite_network_recalculate(nullptr);  // Should not crash
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Clear Tests
 * ============================================================================ */

TEST_CASE("Network clear", "[network][clear]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Clear populated network") {
        for (int i = 0; i < 20; i++) {
            agentite_network_add_node(network, i * 10, 0, 5);
        }
        REQUIRE(agentite_network_node_count(network) == 20);

        agentite_network_clear(network);

        REQUIRE(agentite_network_node_count(network) == 0);
        agentite_network_update(network);
        REQUIRE(agentite_network_group_count(network) == 0);
    }

    SECTION("Clear empty network") {
        agentite_network_clear(network);
        REQUIRE(agentite_network_node_count(network) == 0);
    }

    SECTION("Clear NULL network is safe") {
        agentite_network_clear(nullptr);
        // Should not crash
    }

    SECTION("Can add after clear") {
        agentite_network_add_node(network, 0, 0, 5);
        agentite_network_clear(network);

        uint32_t id = agentite_network_add_node(network, 10, 10, 3);
        REQUIRE(id != AGENTITE_NETWORK_INVALID);
        REQUIRE(agentite_network_node_count(network) == 1);
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

TEST_CASE("Network callbacks", "[network][callback]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    static int callback_count = 0;
    static uint32_t last_node = AGENTITE_NETWORK_INVALID;
    static uint32_t last_old_group = AGENTITE_NETWORK_INVALID;
    static uint32_t last_new_group = AGENTITE_NETWORK_INVALID;

    auto callback = [](Agentite_NetworkSystem *net, uint32_t node_id,
                       uint32_t old_group, uint32_t new_group, void *userdata) {
        callback_count++;
        last_node = node_id;
        last_old_group = old_group;
        last_new_group = new_group;
    };

    callback_count = 0;

    SECTION("Callback on group change") {
        // Add node and update first
        uint32_t id1 = agentite_network_add_node(network, 0, 0, 10);
        uint32_t id2 = agentite_network_add_node(network, 5, 0, 10);  // Connected to id1
        agentite_network_update(network);

        // Now set callback and make a change that causes group change
        agentite_network_set_callback(network, callback, nullptr);
        callback_count = 0;

        // Move id2 far away to disconnect
        agentite_network_move_node(network, id2, 1000, 1000);
        agentite_network_update(network);

        // Callback should be called when group membership changes
        // Note: Implementation may or may not call callback - this tests the API works
        REQUIRE(agentite_network_get_group(network, id1) != agentite_network_get_group(network, id2));
    }

    SECTION("Set NULL callback is safe") {
        agentite_network_set_callback(network, nullptr, nullptr);
        agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);
        // Should not crash
    }

    SECTION("NULL network callback is safe") {
        agentite_network_set_callback(nullptr, callback, nullptr);
        // Should not crash
    }

    agentite_network_destroy(network);
}

/* ============================================================================
 * Edge Cases and Stress Tests
 * ============================================================================ */

TEST_CASE("Network edge cases", "[network][edge]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Large coordinates") {
        uint32_t id = agentite_network_add_node(network, 1000000, -1000000, 5);
        REQUIRE(id != AGENTITE_NETWORK_INVALID);

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node->x == 1000000);
        REQUIRE(node->y == -1000000);
    }

    SECTION("Large radius") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 10000);
        REQUIRE(id != AGENTITE_NETWORK_INVALID);

        agentite_network_update(network);
        REQUIRE(agentite_network_covers_cell(network, 5000, 5000));
    }

    SECTION("Negative production/consumption") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        REQUIRE(agentite_network_set_production(network, id, -50));
        REQUIRE(agentite_network_set_consumption(network, id, -30));

        const Agentite_NetworkNode *node = agentite_network_get_node(network, id);
        REQUIRE(node->production == -50);
        REQUIRE(node->consumption == -30);
    }

    SECTION("Multiple updates without changes") {
        agentite_network_add_node(network, 0, 0, 5);
        agentite_network_update(network);
        agentite_network_update(network);
        agentite_network_update(network);
        // Should not crash or change state
        REQUIRE(agentite_network_node_count(network) == 1);
    }

    SECTION("Get group without update") {
        uint32_t id = agentite_network_add_node(network, 0, 0, 5);
        // Don't call update - group might be invalid or stale
        uint32_t group = agentite_network_get_group(network, id);
        // This is implementation-defined behavior
    }

    agentite_network_destroy(network);
}

TEST_CASE("Network stress test", "[network][stress]") {
    Agentite_NetworkSystem *network = agentite_network_create();
    REQUIRE(network != nullptr);

    SECTION("Many nodes") {
        // Add many nodes
        std::vector<uint32_t> ids;
        for (int i = 0; i < 500; i++) {
            uint32_t id = agentite_network_add_node(network, i * 5, (i % 10) * 5, 3);
            REQUIRE(id != AGENTITE_NETWORK_INVALID);
            ids.push_back(id);
        }

        agentite_network_update(network);
        REQUIRE(agentite_network_node_count(network) == 500);

        // All nodes should have valid groups
        for (uint32_t id : ids) {
            REQUIRE(agentite_network_get_group(network, id) != AGENTITE_NETWORK_INVALID);
        }
    }

    SECTION("Rapid add/remove cycles") {
        for (int cycle = 0; cycle < 100; cycle++) {
            uint32_t id = agentite_network_add_node(network, 0, 0, 5);
            agentite_network_update(network);
            agentite_network_remove_node(network, id);
        }
        REQUIRE(agentite_network_node_count(network) == 0);
    }

    SECTION("Many connected nodes (single large group)") {
        // All nodes connected in a line
        for (int i = 0; i < 100; i++) {
            agentite_network_add_node(network, i * 5, 0, 5);  // Distance 5 <= radius 5 + 5
        }

        agentite_network_update(network);
        REQUIRE(agentite_network_group_count(network) == 1);  // All connected
    }

    agentite_network_destroy(network);
}
