/*
 * Carbon Error Handling Tests
 *
 * Tests for the error reporting system.
 */

#include "catch_amalgamated.hpp"
#include "agentite/error.h"
#include <cstring>

/* ============================================================================
 * Basic Error Operations
 * ============================================================================ */

TEST_CASE("Error set and get", "[error][basic]") {
    // Start with clean state
    agentite_clear_error();

    SECTION("Initial state has no error") {
        REQUIRE_FALSE(agentite_has_error());
        REQUIRE(strlen(agentite_get_last_error()) == 0);
    }

    SECTION("Set simple error") {
        agentite_set_error("Something went wrong");
        REQUIRE(agentite_has_error());
        REQUIRE(strcmp(agentite_get_last_error(), "Something went wrong") == 0);
    }

    SECTION("Set formatted error") {
        agentite_set_error("Failed at line %d: %s", 42, "null pointer");
        REQUIRE(agentite_has_error());
        REQUIRE(strcmp(agentite_get_last_error(), "Failed at line 42: null pointer") == 0);
    }

    SECTION("Clear error") {
        agentite_set_error("An error occurred");
        REQUIRE(agentite_has_error());

        agentite_clear_error();
        REQUIRE_FALSE(agentite_has_error());
        REQUIRE(strlen(agentite_get_last_error()) == 0);
    }

    SECTION("Overwrite existing error") {
        agentite_set_error("First error");
        agentite_set_error("Second error");
        REQUIRE(strcmp(agentite_get_last_error(), "Second error") == 0);
    }

    // Clean up
    agentite_clear_error();
}

/* ============================================================================
 * Format String Tests
 * ============================================================================ */

TEST_CASE("Error format strings", "[error][format]") {
    agentite_clear_error();

    SECTION("Integer formatting") {
        agentite_set_error("Value: %d", 12345);
        REQUIRE(strcmp(agentite_get_last_error(), "Value: 12345") == 0);
    }

    SECTION("Float formatting") {
        agentite_set_error("Value: %.2f", 3.14159);
        REQUIRE(strcmp(agentite_get_last_error(), "Value: 3.14") == 0);
    }

    SECTION("String formatting") {
        agentite_set_error("Name: %s", "Carbon");
        REQUIRE(strcmp(agentite_get_last_error(), "Name: Carbon") == 0);
    }

    SECTION("Multiple format specifiers") {
        agentite_set_error("%s error at %d: code %x", "Memory", 100, 0xDEAD);
        const char *err = agentite_get_last_error();
        REQUIRE(strstr(err, "Memory") != nullptr);
        REQUIRE(strstr(err, "100") != nullptr);
        REQUIRE(strstr(err, "dead") != nullptr);
    }

    agentite_clear_error();
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_CASE("Error edge cases", "[error][edge]") {
    agentite_clear_error();

    SECTION("Empty error string") {
        agentite_set_error("");
        // Empty string is technically set but might not be "has error"
        // Behavior may vary - just verify it doesn't crash
        agentite_get_last_error();
    }

    SECTION("NULL format string") {
        // Should not crash - implementation should handle gracefully
        // This may or may not set an error depending on implementation
        agentite_set_error(nullptr);
        agentite_get_last_error();  // Should not crash
    }

    SECTION("Multiple clears") {
        agentite_clear_error();
        agentite_clear_error();
        agentite_clear_error();
        REQUIRE_FALSE(agentite_has_error());
    }

    SECTION("Get error multiple times") {
        agentite_set_error("Test error");
        const char *err1 = agentite_get_last_error();
        const char *err2 = agentite_get_last_error();
        REQUIRE(strcmp(err1, err2) == 0);
        REQUIRE(err1 == err2);  // Should be same pointer (thread-local buffer)
    }

    agentite_clear_error();
}
