/*
 * Agentite Technology Tree Tests
 *
 * Tests for the tech tree system including registration, research,
 * prerequisites, and state management.
 */

#include "catch_amalgamated.hpp"
#include "agentite/tech.h"
#include <cstring>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static Agentite_TechDef create_basic_tech(const char *id, const char *name, int cost) {
    Agentite_TechDef tech = {};
    strncpy(tech.id, id, sizeof(tech.id) - 1);
    strncpy(tech.name, name, sizeof(tech.name) - 1);
    tech.research_cost = cost;
    tech.branch = 0;
    tech.tier = 0;
    return tech;
}

static Agentite_TechDef create_tech_with_prereq(const char *id, const char *name,
                                                int cost, const char *prereq) {
    Agentite_TechDef tech = create_basic_tech(id, name, cost);
    strncpy(tech.prerequisites[0], prereq, sizeof(tech.prerequisites[0]) - 1);
    tech.prereq_count = 1;
    return tech;
}

/* ============================================================================
 * Tech Tree Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Tech tree lifecycle", "[tech][lifecycle]") {
    SECTION("Create and destroy tech tree") {
        Agentite_TechTree *tree = agentite_tech_create();
        REQUIRE(tree != nullptr);
        agentite_tech_destroy(tree);
    }

    SECTION("Destroy NULL tree is safe") {
        agentite_tech_destroy(nullptr);
        // Should not crash
    }

    SECTION("Empty tree has zero techs") {
        Agentite_TechTree *tree = agentite_tech_create();
        REQUIRE(agentite_tech_count(tree) == 0);
        agentite_tech_destroy(tree);
    }
}

/* ============================================================================
 * Tech Registration Tests
 * ============================================================================ */

TEST_CASE("Tech registration", "[tech][register]") {
    Agentite_TechTree *tree = agentite_tech_create();
    REQUIRE(tree != nullptr);

    SECTION("Register single tech") {
        Agentite_TechDef tech = create_basic_tech("farming", "Farming", 100);

        int index = agentite_tech_register(tree, &tech);
        REQUIRE(index >= 0);
        REQUIRE(agentite_tech_count(tree) == 1);
    }

    SECTION("Register multiple techs") {
        Agentite_TechDef tech1 = create_basic_tech("farming", "Farming", 100);
        Agentite_TechDef tech2 = create_basic_tech("mining", "Mining", 150);
        Agentite_TechDef tech3 = create_basic_tech("writing", "Writing", 200);

        int idx1 = agentite_tech_register(tree, &tech1);
        int idx2 = agentite_tech_register(tree, &tech2);
        int idx3 = agentite_tech_register(tree, &tech3);

        REQUIRE(idx1 >= 0);
        REQUIRE(idx2 >= 0);
        REQUIRE(idx3 >= 0);
        REQUIRE(agentite_tech_count(tree) == 3);
    }

    SECTION("Get tech by index") {
        Agentite_TechDef tech = create_basic_tech("test_tech", "Test Tech", 100);
        int index = agentite_tech_register(tree, &tech);

        const Agentite_TechDef *retrieved = agentite_tech_get(tree, index);
        REQUIRE(retrieved != nullptr);
        REQUIRE(strcmp(retrieved->id, "test_tech") == 0);
        REQUIRE(strcmp(retrieved->name, "Test Tech") == 0);
        REQUIRE(retrieved->research_cost == 100);
    }

    SECTION("Get tech with invalid index returns NULL") {
        const Agentite_TechDef *tech = agentite_tech_get(tree, 999);
        REQUIRE(tech == nullptr);

        tech = agentite_tech_get(tree, -1);
        REQUIRE(tech == nullptr);
    }

    SECTION("Find tech by ID") {
        Agentite_TechDef tech = create_basic_tech("unique_id", "Unique Tech", 50);
        agentite_tech_register(tree, &tech);

        const Agentite_TechDef *found = agentite_tech_find(tree, "unique_id");
        REQUIRE(found != nullptr);
        REQUIRE(strcmp(found->id, "unique_id") == 0);
    }

    SECTION("Find tech with unknown ID returns NULL") {
        const Agentite_TechDef *found = agentite_tech_find(tree, "nonexistent");
        REQUIRE(found == nullptr);
    }

    SECTION("Find tech index by ID") {
        Agentite_TechDef tech = create_basic_tech("indexed_tech", "Indexed Tech", 75);
        int registered_index = agentite_tech_register(tree, &tech);

        int found_index = agentite_tech_find_index(tree, "indexed_tech");
        REQUIRE(found_index == registered_index);
    }

    SECTION("Find index with unknown ID returns -1") {
        int index = agentite_tech_find_index(tree, "nonexistent");
        REQUIRE(index == -1);
    }

    agentite_tech_destroy(tree);
}

/* ============================================================================
 * Tech State Tests
 * ============================================================================ */

TEST_CASE("Tech state management", "[tech][state]") {
    SECTION("Initialize state") {
        Agentite_TechState state;
        agentite_tech_state_init(&state);

        REQUIRE(state.completed_count == 0);
        REQUIRE(state.active_count == 0);
    }

    SECTION("Reset state") {
        Agentite_TechState state;
        agentite_tech_state_init(&state);

        // Simulate some progress
        state.completed_count = 5;
        state.active_count = 2;

        agentite_tech_state_reset(&state);

        REQUIRE(state.completed_count == 0);
        REQUIRE(state.active_count == 0);
    }

    SECTION("Initialize NULL state is safe") {
        agentite_tech_state_init(nullptr);
        // Should not crash
    }

    SECTION("Reset NULL state is safe") {
        agentite_tech_state_reset(nullptr);
        // Should not crash
    }
}

/* ============================================================================
 * Research Operation Tests
 * ============================================================================ */

TEST_CASE("Tech research operations", "[tech][research]") {
    Agentite_TechTree *tree = agentite_tech_create();
    REQUIRE(tree != nullptr);

    Agentite_TechDef farming = create_basic_tech("farming", "Farming", 100);
    Agentite_TechDef irrigation = create_tech_with_prereq("irrigation", "Irrigation", 150, "farming");

    agentite_tech_register(tree, &farming);
    agentite_tech_register(tree, &irrigation);

    Agentite_TechState state;
    agentite_tech_state_init(&state);

    SECTION("Check is_researched for new state") {
        REQUIRE_FALSE(agentite_tech_is_researched(tree, &state, "farming"));
        REQUIRE_FALSE(agentite_tech_is_researched(tree, &state, "irrigation"));
    }

    SECTION("Check can_research without prerequisites") {
        REQUIRE(agentite_tech_can_research(tree, &state, "farming"));
    }

    SECTION("Check can_research with missing prerequisite") {
        REQUIRE_FALSE(agentite_tech_can_research(tree, &state, "irrigation"));
    }

    SECTION("Check has_prerequisites") {
        REQUIRE(agentite_tech_has_prerequisites(tree, &state, "farming"));
        REQUIRE_FALSE(agentite_tech_has_prerequisites(tree, &state, "irrigation"));
    }

    SECTION("Start research") {
        bool started = agentite_tech_start_research(tree, &state, "farming");
        REQUIRE(started);
        REQUIRE(agentite_tech_active_count(&state) == 1);
        REQUIRE(agentite_tech_is_researching(&state, "farming"));
    }

    SECTION("Cannot start research without prerequisites") {
        bool started = agentite_tech_start_research(tree, &state, "irrigation");
        REQUIRE_FALSE(started);
    }

    SECTION("Add research points completes tech") {
        agentite_tech_start_research(tree, &state, "farming");

        // Add enough points to complete
        bool completed = agentite_tech_add_points(tree, &state, 100);
        REQUIRE(completed);
        REQUIRE(agentite_tech_is_researched(tree, &state, "farming"));
    }

    SECTION("Partial research progress") {
        agentite_tech_start_research(tree, &state, "farming");

        bool completed = agentite_tech_add_points(tree, &state, 50);
        REQUIRE_FALSE(completed);

        float progress = agentite_tech_get_progress(&state, 0);
        REQUIRE(progress > 0.0f);
        REQUIRE(progress < 1.0f);

        int remaining = agentite_tech_get_remaining(&state, 0);
        REQUIRE(remaining == 50);
    }

    SECTION("Can research after prerequisite completed") {
        // Complete farming first
        agentite_tech_complete(tree, &state, "farming");

        REQUIRE(agentite_tech_has_prerequisites(tree, &state, "irrigation"));
        REQUIRE(agentite_tech_can_research(tree, &state, "irrigation"));
    }

    SECTION("Cancel research") {
        agentite_tech_start_research(tree, &state, "farming");
        REQUIRE(agentite_tech_active_count(&state) == 1);

        agentite_tech_cancel_research(&state, 0);
        REQUIRE(agentite_tech_active_count(&state) == 0);
    }

    SECTION("Cancel all research") {
        // Register more techs
        Agentite_TechDef mining = create_basic_tech("mining", "Mining", 100);
        agentite_tech_register(tree, &mining);

        agentite_tech_start_research(tree, &state, "farming");
        agentite_tech_start_research(tree, &state, "mining");

        agentite_tech_cancel_all_research(&state);
        REQUIRE(agentite_tech_active_count(&state) == 0);
    }

    agentite_tech_destroy(tree);
}

/* ============================================================================
 * Complete Tech Tests
 * ============================================================================ */

TEST_CASE("Tech completion", "[tech][complete]") {
    Agentite_TechTree *tree = agentite_tech_create();
    Agentite_TechDef tech = create_basic_tech("test", "Test", 100);
    agentite_tech_register(tree, &tech);

    Agentite_TechState state;
    agentite_tech_state_init(&state);

    SECTION("Complete tech immediately") {
        agentite_tech_complete(tree, &state, "test");
        REQUIRE(agentite_tech_is_researched(tree, &state, "test"));
    }

    SECTION("Completed tech cannot be researched again (non-repeatable)") {
        agentite_tech_complete(tree, &state, "test");
        REQUIRE_FALSE(agentite_tech_can_research(tree, &state, "test"));
    }

    agentite_tech_destroy(tree);
}

/* ============================================================================
 * Repeatable Tech Tests
 * ============================================================================ */

TEST_CASE("Repeatable tech", "[tech][repeatable]") {
    Agentite_TechTree *tree = agentite_tech_create();

    Agentite_TechDef tech = create_basic_tech("upgrade", "Upgrade", 50);
    tech.repeatable = true;
    agentite_tech_register(tree, &tech);

    Agentite_TechState state;
    agentite_tech_state_init(&state);

    SECTION("Repeatable tech can be researched multiple times") {
        agentite_tech_complete(tree, &state, "upgrade");
        REQUIRE(agentite_tech_can_research(tree, &state, "upgrade"));

        agentite_tech_complete(tree, &state, "upgrade");
        REQUIRE(agentite_tech_can_research(tree, &state, "upgrade"));
    }

    SECTION("Get repeat count") {
        REQUIRE(agentite_tech_get_repeat_count(tree, &state, "upgrade") == 0);

        agentite_tech_complete(tree, &state, "upgrade");
        REQUIRE(agentite_tech_get_repeat_count(tree, &state, "upgrade") == 1);

        agentite_tech_complete(tree, &state, "upgrade");
        REQUIRE(agentite_tech_get_repeat_count(tree, &state, "upgrade") == 2);
    }

    agentite_tech_destroy(tree);
}

/* ============================================================================
 * Query Function Tests
 * ============================================================================ */

TEST_CASE("Tech query functions", "[tech][query]") {
    Agentite_TechTree *tree = agentite_tech_create();

    Agentite_TechDef tech1 = create_basic_tech("tier0_a", "Tier 0 A", 100);
    tech1.branch = 1;
    tech1.tier = 0;

    Agentite_TechDef tech2 = create_basic_tech("tier0_b", "Tier 0 B", 100);
    tech2.branch = 2;
    tech2.tier = 0;

    Agentite_TechDef tech3 = create_basic_tech("tier1_a", "Tier 1 A", 200);
    tech3.branch = 1;
    tech3.tier = 1;

    agentite_tech_register(tree, &tech1);
    agentite_tech_register(tree, &tech2);
    agentite_tech_register(tree, &tech3);

    Agentite_TechState state;
    agentite_tech_state_init(&state);

    SECTION("Get available techs") {
        const Agentite_TechDef *available[10];
        int count = agentite_tech_get_available(tree, &state, available, 10);

        // All tier 0 techs should be available
        REQUIRE(count >= 2);
    }

    SECTION("Get completed techs") {
        agentite_tech_complete(tree, &state, "tier0_a");

        const Agentite_TechDef *completed[10];
        int count = agentite_tech_get_completed(tree, &state, completed, 10);

        REQUIRE(count == 1);
        REQUIRE(strcmp(completed[0]->id, "tier0_a") == 0);
    }

    SECTION("Get techs by branch") {
        const Agentite_TechDef *branch1[10];
        int count = agentite_tech_get_by_branch(tree, 1, branch1, 10);

        REQUIRE(count == 2);  // tier0_a and tier1_a
    }

    SECTION("Get techs by tier") {
        const Agentite_TechDef *tier0[10];
        int count = agentite_tech_get_by_tier(tree, 0, tier0, 10);

        REQUIRE(count == 2);  // tier0_a and tier0_b
    }

    agentite_tech_destroy(tree);
}

/* ============================================================================
 * Effect Type Tests
 * ============================================================================ */

TEST_CASE("Tech effect type names", "[tech][effects]") {
    SECTION("Effect types have names") {
        const char *name;

        name = agentite_tech_effect_type_name(AGENTITE_TECH_EFFECT_NONE);
        REQUIRE(name != nullptr);

        name = agentite_tech_effect_type_name(AGENTITE_TECH_EFFECT_RESOURCE_BONUS);
        REQUIRE(name != nullptr);

        name = agentite_tech_effect_type_name(AGENTITE_TECH_EFFECT_ATTACK_BONUS);
        REQUIRE(name != nullptr);

        name = agentite_tech_effect_type_name(AGENTITE_TECH_EFFECT_UNLOCK_UNIT);
        REQUIRE(name != nullptr);
    }

    SECTION("Unknown effect type returns fallback") {
        const char *name = agentite_tech_effect_type_name((Agentite_TechEffectType)999);
        REQUIRE(name != nullptr);
    }
}

/* ============================================================================
 * Cost Calculation Tests
 * ============================================================================ */

TEST_CASE("Tech cost calculation", "[tech][cost]") {
    SECTION("Basic cost calculation") {
        Agentite_TechDef tech = create_basic_tech("test", "Test", 100);
        int cost = agentite_tech_calculate_cost(&tech, 0);
        REQUIRE(cost == 100);
    }

    SECTION("Cost may increase with repeat count") {
        Agentite_TechDef tech = create_basic_tech("test", "Test", 100);
        tech.repeatable = true;

        int cost0 = agentite_tech_calculate_cost(&tech, 0);
        int cost1 = agentite_tech_calculate_cost(&tech, 1);
        int cost2 = agentite_tech_calculate_cost(&tech, 2);

        // Cost should generally increase (or at least not decrease)
        REQUIRE(cost1 >= cost0);
        REQUIRE(cost2 >= cost1);
    }

    SECTION("NULL tech returns 0") {
        int cost = agentite_tech_calculate_cost(nullptr, 0);
        REQUIRE(cost == 0);
    }
}

/* ============================================================================
 * NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tech NULL safety", "[tech][null]") {
    Agentite_TechTree *tree = agentite_tech_create();
    Agentite_TechState state;
    agentite_tech_state_init(&state);

    SECTION("Functions with NULL tree") {
        REQUIRE(agentite_tech_count(nullptr) == 0);
        REQUIRE(agentite_tech_get(nullptr, 0) == nullptr);
        REQUIRE(agentite_tech_find(nullptr, "test") == nullptr);
        REQUIRE(agentite_tech_find_index(nullptr, "test") == -1);
    }

    SECTION("Research functions with NULL tree") {
        REQUIRE_FALSE(agentite_tech_is_researched(nullptr, &state, "test"));
        REQUIRE_FALSE(agentite_tech_can_research(nullptr, &state, "test"));
        REQUIRE_FALSE(agentite_tech_start_research(nullptr, &state, "test"));
    }

    SECTION("Research functions with NULL state") {
        REQUIRE_FALSE(agentite_tech_is_researched(tree, nullptr, "test"));
        REQUIRE_FALSE(agentite_tech_can_research(tree, nullptr, "test"));
        REQUIRE_FALSE(agentite_tech_start_research(tree, nullptr, "test"));
    }

    SECTION("Query functions with NULL arguments") {
        const Agentite_TechDef *out[10];
        REQUIRE(agentite_tech_get_available(nullptr, &state, out, 10) == 0);
        REQUIRE(agentite_tech_get_completed(nullptr, &state, out, 10) == 0);
        REQUIRE(agentite_tech_get_by_branch(nullptr, 0, out, 10) == 0);
        REQUIRE(agentite_tech_get_by_tier(nullptr, 0, out, 10) == 0);
    }

    SECTION("State query functions with NULL state") {
        REQUIRE(agentite_tech_active_count(nullptr) == 0);
        REQUIRE(agentite_tech_get_progress(nullptr, 0) == 0.0f);
        REQUIRE(agentite_tech_get_remaining(nullptr, 0) == 0);
        REQUIRE_FALSE(agentite_tech_is_researching(nullptr, "test"));
    }

    agentite_tech_destroy(tree);
}

/* ============================================================================
 * Constant Tests
 * ============================================================================ */

TEST_CASE("Tech constants", "[tech][constants]") {
    SECTION("Maximum techs is reasonable") {
        REQUIRE(AGENTITE_TECH_MAX >= 64);
        REQUIRE(AGENTITE_TECH_MAX <= 1024);
    }

    SECTION("Maximum prerequisites is reasonable") {
        REQUIRE(AGENTITE_TECH_MAX_PREREQS >= 2);
        REQUIRE(AGENTITE_TECH_MAX_PREREQS <= 8);
    }

    SECTION("Maximum effects is reasonable") {
        REQUIRE(AGENTITE_TECH_MAX_EFFECTS >= 2);
        REQUIRE(AGENTITE_TECH_MAX_EFFECTS <= 16);
    }

    SECTION("Maximum active research slots is reasonable") {
        REQUIRE(AGENTITE_TECH_MAX_ACTIVE >= 1);
        REQUIRE(AGENTITE_TECH_MAX_ACTIVE <= 8);
    }
}

/* ============================================================================
 * Integration Test
 * ============================================================================ */

TEST_CASE("Tech tree integration", "[tech][integration]") {
    Agentite_TechTree *tree = agentite_tech_create();
    REQUIRE(tree != nullptr);

    // Create a simple tech tree:
    // farming -> irrigation -> advanced_irrigation
    //        \-> animal_husbandry

    Agentite_TechDef farming = create_basic_tech("farming", "Farming", 50);
    Agentite_TechDef irrigation = create_tech_with_prereq("irrigation", "Irrigation", 100, "farming");
    Agentite_TechDef adv_irrigation = create_tech_with_prereq("adv_irrigation", "Advanced Irrigation", 200, "irrigation");
    Agentite_TechDef husbandry = create_tech_with_prereq("husbandry", "Animal Husbandry", 100, "farming");

    agentite_tech_register(tree, &farming);
    agentite_tech_register(tree, &irrigation);
    agentite_tech_register(tree, &adv_irrigation);
    agentite_tech_register(tree, &husbandry);

    Agentite_TechState state;
    agentite_tech_state_init(&state);

    SECTION("Research path simulation") {
        // Initially only farming is available
        REQUIRE(agentite_tech_can_research(tree, &state, "farming"));
        REQUIRE_FALSE(agentite_tech_can_research(tree, &state, "irrigation"));
        REQUIRE_FALSE(agentite_tech_can_research(tree, &state, "husbandry"));

        // Research farming
        agentite_tech_start_research(tree, &state, "farming");
        agentite_tech_add_points(tree, &state, 50);
        REQUIRE(agentite_tech_is_researched(tree, &state, "farming"));

        // Now irrigation and husbandry are available
        REQUIRE(agentite_tech_can_research(tree, &state, "irrigation"));
        REQUIRE(agentite_tech_can_research(tree, &state, "husbandry"));
        REQUIRE_FALSE(agentite_tech_can_research(tree, &state, "adv_irrigation"));

        // Research irrigation
        agentite_tech_start_research(tree, &state, "irrigation");
        agentite_tech_add_points(tree, &state, 100);
        REQUIRE(agentite_tech_is_researched(tree, &state, "irrigation"));

        // Now advanced irrigation is available
        REQUIRE(agentite_tech_can_research(tree, &state, "adv_irrigation"));
    }

    agentite_tech_destroy(tree);
}
