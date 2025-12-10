/*
 * Carbon Resource Management Tests
 *
 * Tests for the resource management system including
 * initialization, spending, adding, per-turn ticks, and modifiers.
 */

#include "catch_amalgamated.hpp"
#include "agentite/resource.h"

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST_CASE("Resource initialization", "[resource][init]") {
    SECTION("Basic initialization") {
        Agentite_Resource r;
        agentite_resource_init(&r, 100, 500, 10);

        REQUIRE(r.current == 100);
        REQUIRE(r.maximum == 500);
        REQUIRE(r.per_turn_base == 10);
        REQUIRE(r.per_turn_modifier == 1.0f);
    }

    SECTION("Unlimited maximum (0)") {
        Agentite_Resource r;
        agentite_resource_init(&r, 50, 0, 5);

        REQUIRE(r.current == 50);
        REQUIRE(r.maximum == 0);
    }

    SECTION("Zero per-turn") {
        Agentite_Resource r;
        agentite_resource_init(&r, 100, 100, 0);

        REQUIRE(r.per_turn_base == 0);
    }

    SECTION("NULL pointer is safe") {
        agentite_resource_init(nullptr, 100, 500, 10);
        // Should not crash
    }
}

/* ============================================================================
 * Spending Tests
 * ============================================================================ */

TEST_CASE("Resource spending", "[resource][spend]") {
    Agentite_Resource r;
    agentite_resource_init(&r, 100, 500, 10);

    SECTION("Can afford check") {
        REQUIRE(agentite_resource_can_afford(&r, 50));
        REQUIRE(agentite_resource_can_afford(&r, 100));
        REQUIRE_FALSE(agentite_resource_can_afford(&r, 101));
        REQUIRE_FALSE(agentite_resource_can_afford(&r, 1000));
    }

    SECTION("Can afford zero") {
        REQUIRE(agentite_resource_can_afford(&r, 0));
    }

    SECTION("Successful spend") {
        REQUIRE(agentite_resource_spend(&r, 30));
        REQUIRE(r.current == 70);

        REQUIRE(agentite_resource_spend(&r, 70));
        REQUIRE(r.current == 0);
    }

    SECTION("Spend exact amount") {
        REQUIRE(agentite_resource_spend(&r, 100));
        REQUIRE(r.current == 0);
    }

    SECTION("Failed spend leaves resource unchanged") {
        REQUIRE_FALSE(agentite_resource_spend(&r, 150));
        REQUIRE(r.current == 100);
    }

    SECTION("Spend zero") {
        REQUIRE(agentite_resource_spend(&r, 0));
        REQUIRE(r.current == 100);
    }

    SECTION("Spend negative returns false") {
        REQUIRE_FALSE(agentite_resource_spend(&r, -10));
        REQUIRE(r.current == 100);
    }

    SECTION("NULL pointer returns false") {
        REQUIRE_FALSE(agentite_resource_can_afford(nullptr, 10));
        REQUIRE_FALSE(agentite_resource_spend(nullptr, 10));
    }
}

/* ============================================================================
 * Adding Tests
 * ============================================================================ */

TEST_CASE("Resource adding", "[resource][add]") {
    Agentite_Resource r;
    agentite_resource_init(&r, 100, 500, 10);

    SECTION("Basic add") {
        agentite_resource_add(&r, 50);
        REQUIRE(r.current == 150);
    }

    SECTION("Add up to maximum") {
        agentite_resource_add(&r, 400);
        REQUIRE(r.current == 500);
    }

    SECTION("Add past maximum clamps") {
        agentite_resource_add(&r, 1000);
        REQUIRE(r.current == 500);
    }

    SECTION("Add with unlimited maximum (0)") {
        Agentite_Resource unlimited;
        agentite_resource_init(&unlimited, 100, 0, 10);

        agentite_resource_add(&unlimited, 10000);
        REQUIRE(unlimited.current == 10100);
    }

    SECTION("Add negative reduces resource") {
        agentite_resource_add(&r, -30);
        REQUIRE(r.current == 70);
    }

    SECTION("Add negative clamps to zero") {
        agentite_resource_add(&r, -200);
        REQUIRE(r.current == 0);
    }

    SECTION("Add zero does nothing") {
        agentite_resource_add(&r, 0);
        REQUIRE(r.current == 100);
    }

    SECTION("NULL pointer is safe") {
        agentite_resource_add(nullptr, 50);
    }
}

/* ============================================================================
 * Set Tests
 * ============================================================================ */

TEST_CASE("Resource set values", "[resource][set]") {
    Agentite_Resource r;
    agentite_resource_init(&r, 100, 500, 10);

    SECTION("Set current value") {
        agentite_resource_set(&r, 250);
        REQUIRE(r.current == 250);
    }

    SECTION("Set above maximum clamps") {
        agentite_resource_set(&r, 1000);
        REQUIRE(r.current == 500);
    }

    SECTION("Set negative clamps to zero") {
        agentite_resource_set(&r, -50);
        REQUIRE(r.current == 0);
    }

    SECTION("Set to zero") {
        agentite_resource_set(&r, 0);
        REQUIRE(r.current == 0);
    }

    SECTION("Set modifier") {
        agentite_resource_set_modifier(&r, 2.0f);
        REQUIRE(r.per_turn_modifier == 2.0f);

        agentite_resource_set_modifier(&r, 0.5f);
        REQUIRE(r.per_turn_modifier == 0.5f);
    }

    SECTION("Set per turn") {
        agentite_resource_set_per_turn(&r, 25);
        REQUIRE(r.per_turn_base == 25);
    }

    SECTION("Set maximum") {
        agentite_resource_set_max(&r, 200);
        REQUIRE(r.maximum == 200);
        REQUIRE(r.current == 100);  // Still below new max
    }

    SECTION("Set maximum clamps current") {
        r.current = 400;
        agentite_resource_set_max(&r, 200);
        REQUIRE(r.maximum == 200);
        REQUIRE(r.current == 200);  // Clamped to new max
    }

    SECTION("Set unlimited maximum") {
        agentite_resource_set_max(&r, 0);
        REQUIRE(r.maximum == 0);
    }

    SECTION("NULL pointer is safe") {
        agentite_resource_set(nullptr, 100);
        agentite_resource_set_modifier(nullptr, 2.0f);
        agentite_resource_set_per_turn(nullptr, 25);
        agentite_resource_set_max(nullptr, 200);
    }
}

/* ============================================================================
 * Per-Turn Tick Tests
 * ============================================================================ */

TEST_CASE("Resource per-turn tick", "[resource][tick]") {
    Agentite_Resource r;
    agentite_resource_init(&r, 100, 500, 10);

    SECTION("Basic tick") {
        agentite_resource_tick(&r);
        REQUIRE(r.current == 110);
    }

    SECTION("Multiple ticks") {
        for (int i = 0; i < 5; i++) {
            agentite_resource_tick(&r);
        }
        REQUIRE(r.current == 150);
    }

    SECTION("Tick respects maximum") {
        r.current = 495;
        agentite_resource_tick(&r);
        REQUIRE(r.current == 500);
    }

    SECTION("Tick with modifier > 1") {
        agentite_resource_set_modifier(&r, 2.0f);
        agentite_resource_tick(&r);
        REQUIRE(r.current == 120);  // 100 + 10 * 2
    }

    SECTION("Tick with modifier < 1") {
        agentite_resource_set_modifier(&r, 0.5f);
        agentite_resource_tick(&r);
        REQUIRE(r.current == 105);  // 100 + 10 * 0.5
    }

    SECTION("Tick with zero modifier") {
        agentite_resource_set_modifier(&r, 0.0f);
        agentite_resource_tick(&r);
        REQUIRE(r.current == 100);
    }

    SECTION("Tick with negative modifier (drain)") {
        agentite_resource_set_modifier(&r, -1.0f);
        agentite_resource_tick(&r);
        REQUIRE(r.current == 90);  // 100 + 10 * -1
    }

    SECTION("Tick with negative per_turn") {
        agentite_resource_set_per_turn(&r, -5);
        agentite_resource_tick(&r);
        REQUIRE(r.current == 95);
    }

    SECTION("Tick with negative per_turn drains to zero") {
        agentite_resource_set_per_turn(&r, -200);
        agentite_resource_tick(&r);
        REQUIRE(r.current == 0);
    }

    SECTION("Tick with unlimited maximum") {
        Agentite_Resource unlimited;
        agentite_resource_init(&unlimited, 100, 0, 100);

        for (int i = 0; i < 100; i++) {
            agentite_resource_tick(&unlimited);
        }
        REQUIRE(unlimited.current == 10100);
    }

    SECTION("NULL pointer is safe") {
        agentite_resource_tick(nullptr);
    }
}

/* ============================================================================
 * Preview Tick Tests
 * ============================================================================ */

TEST_CASE("Resource preview tick", "[resource][preview]") {
    Agentite_Resource r;
    agentite_resource_init(&r, 100, 500, 10);

    SECTION("Basic preview") {
        int preview = agentite_resource_preview_tick(&r);
        REQUIRE(preview == 10);
    }

    SECTION("Preview with modifier") {
        agentite_resource_set_modifier(&r, 2.5f);
        int preview = agentite_resource_preview_tick(&r);
        REQUIRE(preview == 25);
    }

    SECTION("Preview with zero modifier") {
        agentite_resource_set_modifier(&r, 0.0f);
        int preview = agentite_resource_preview_tick(&r);
        REQUIRE(preview == 0);
    }

    SECTION("Preview doesn't change current") {
        int before = r.current;
        agentite_resource_preview_tick(&r);
        REQUIRE(r.current == before);
    }

    SECTION("NULL pointer returns 0") {
        int preview = agentite_resource_preview_tick(nullptr);
        REQUIRE(preview == 0);
    }
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_CASE("Resource economy simulation", "[resource][integration]") {
    // Simulate a simple economy over several turns

    Agentite_Resource gold;
    Agentite_Resource food;

    agentite_resource_init(&gold, 1000, 10000, 50);   // 1000 gold, +50/turn
    agentite_resource_init(&food, 100, 500, 20);      // 100 food, +20/turn

    SECTION("Economy simulation over 10 turns") {
        for (int turn = 0; turn < 10; turn++) {
            // Simulate expenses
            if (turn % 2 == 0) {
                agentite_resource_spend(&gold, 30);  // Maintenance
            }
            agentite_resource_spend(&food, 15);  // Feeding units

            // Per-turn income
            agentite_resource_tick(&gold);
            agentite_resource_tick(&food);
        }

        // After 10 turns:
        // Gold: 1000 - 5*30 (150) + 10*50 (500) = 1350
        REQUIRE(gold.current == 1350);

        // Food: 100 - 10*15 (150) + 10*20 (200) = 150
        REQUIRE(food.current == 150);
    }

    SECTION("Economy with production boost") {
        // Apply a 50% boost to gold production
        agentite_resource_set_modifier(&gold, 1.5f);

        for (int turn = 0; turn < 5; turn++) {
            agentite_resource_tick(&gold);
        }

        // After 5 turns: 1000 + 5 * (50 * 1.5) = 1000 + 375 = 1375
        REQUIRE(gold.current == 1375);
    }

    SECTION("Resource capped at maximum") {
        // Set food to near maximum
        agentite_resource_set(&food, 490);

        // Multiple ticks should cap at 500
        for (int turn = 0; turn < 10; turn++) {
            agentite_resource_tick(&food);
        }

        REQUIRE(food.current == 500);
    }
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_CASE("Resource edge cases", "[resource][edge]") {
    SECTION("Very large values") {
        Agentite_Resource r;
        agentite_resource_init(&r, 1000000000, 0, 1000000);

        agentite_resource_tick(&r);
        REQUIRE(r.current == 1001000000);
    }

    SECTION("Maximum equals current") {
        Agentite_Resource r;
        agentite_resource_init(&r, 100, 100, 10);

        agentite_resource_tick(&r);
        REQUIRE(r.current == 100);  // Capped
    }

    SECTION("Zero current") {
        Agentite_Resource r;
        agentite_resource_init(&r, 0, 100, 10);

        REQUIRE_FALSE(agentite_resource_can_afford(&r, 1));
        REQUIRE_FALSE(agentite_resource_spend(&r, 1));

        agentite_resource_tick(&r);
        REQUIRE(r.current == 10);
    }

    SECTION("Fractional modifier rounding") {
        Agentite_Resource r;
        agentite_resource_init(&r, 100, 500, 10);

        // Test fractional results get truncated to int
        agentite_resource_set_modifier(&r, 1.7f);
        int preview = agentite_resource_preview_tick(&r);
        REQUIRE(preview == 17);

        agentite_resource_set_modifier(&r, 1.3f);
        preview = agentite_resource_preview_tick(&r);
        REQUIRE(preview == 13);
    }

    SECTION("Spend and add in same turn") {
        Agentite_Resource r;
        agentite_resource_init(&r, 100, 500, 10);

        // Complex operation sequence
        agentite_resource_spend(&r, 50);   // 50
        agentite_resource_add(&r, 30);     // 80
        agentite_resource_tick(&r);        // 90
        agentite_resource_spend(&r, 40);   // 50

        REQUIRE(r.current == 50);
    }

    SECTION("Multiple modifiers applied via set") {
        Agentite_Resource r;
        agentite_resource_init(&r, 100, 500, 10);

        agentite_resource_set_modifier(&r, 2.0f);
        REQUIRE(r.per_turn_modifier == 2.0f);

        agentite_resource_set_modifier(&r, 3.0f);
        REQUIRE(r.per_turn_modifier == 3.0f);

        agentite_resource_tick(&r);
        REQUIRE(r.current == 130);  // 100 + 10 * 3
    }
}
