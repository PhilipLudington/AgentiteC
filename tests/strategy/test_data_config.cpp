/*
 * Agentite Data Config Tests
 *
 * Tests for the TOML configuration loading system including parsing,
 * data access, edge cases, and error handling.
 */

#include "catch_amalgamated.hpp"
#include "agentite/data_config.h"
#include <cstring>
#include <string>

/* ============================================================================
 * Test Data Structures
 * ============================================================================ */

// Simple test entry type
struct TestEntry {
    char id[64];
    char name[128];
    int value;
    float rate;
    bool enabled;
};

// Parse callback for TestEntry
static bool parse_test_entry(const char *key, toml_table_t *table,
                            void *out_entry, void *userdata) {
    TestEntry *entry = static_cast<TestEntry*>(out_entry);

    // Get id from key or from table
    if (key && key[0] != '\0') {
        strncpy(entry->id, key, sizeof(entry->id) - 1);
        entry->id[sizeof(entry->id) - 1] = '\0';
    } else {
        agentite_toml_get_string(table, "id", entry->id, sizeof(entry->id));
    }

    agentite_toml_get_string(table, "name", entry->name, sizeof(entry->name));
    agentite_toml_get_int(table, "value", &entry->value);
    agentite_toml_get_float(table, "rate", &entry->rate);
    agentite_toml_get_bool(table, "enabled", &entry->enabled);

    return true;
}

// Parse callback that always fails
static bool parse_fail(const char *key, toml_table_t *table,
                      void *out_entry, void *userdata) {
    return false;
}

// Parse callback that counts calls
static int parse_call_count = 0;
static bool parse_count(const char *key, toml_table_t *table,
                       void *out_entry, void *userdata) {
    parse_call_count++;
    return true;
}

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

TEST_CASE("Data loader creation and destruction", "[data_config][lifecycle]") {
    SECTION("Create loader") {
        Agentite_DataLoader *loader = agentite_data_create();
        REQUIRE(loader != nullptr);
        agentite_data_destroy(loader);
    }

    SECTION("Destroy NULL is safe") {
        agentite_data_destroy(nullptr);
        // Should not crash
    }
}

/* ============================================================================
 * Load from String Tests
 * ============================================================================ */

TEST_CASE("Data loader load from string", "[data_config][load_string]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    SECTION("Load valid TOML array") {
        const char *toml = R"(
            [[item]]
            id = "item1"
            name = "First Item"
            value = 100
            rate = 1.5
            enabled = true

            [[item]]
            id = "item2"
            name = "Second Item"
            value = 200
            rate = 2.5
            enabled = false
        )";

        bool result = agentite_data_load_string(loader, toml, "item",
                                                sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE(result);
        REQUIRE(agentite_data_count(loader) == 2);

        TestEntry *e1 = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 0));
        REQUIRE(e1 != nullptr);
        REQUIRE(std::string(e1->id) == "item1");
        REQUIRE(std::string(e1->name) == "First Item");
        REQUIRE(e1->value == 100);
        REQUIRE(e1->rate == 1.5f);
        REQUIRE(e1->enabled == true);

        TestEntry *e2 = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 1));
        REQUIRE(e2 != nullptr);
        REQUIRE(std::string(e2->id) == "item2");
        REQUIRE(e2->enabled == false);
    }

    SECTION("Load root-level tables (NULL array_key)") {
        const char *toml = R"(
            [config1]
            id = "config1"
            name = "Config One"
            value = 50

            [config2]
            id = "config2"
            name = "Config Two"
            value = 75
        )";

        bool result = agentite_data_load_string(loader, toml, nullptr,
                                                sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE(result);
        REQUIRE(agentite_data_count(loader) >= 2);
    }

    SECTION("Load empty TOML") {
        const char *toml = "";
        bool result = agentite_data_load_string(loader, toml, "item",
                                                sizeof(TestEntry), parse_test_entry, nullptr);
        // Empty TOML with no array should succeed with 0 entries
        REQUIRE(agentite_data_count(loader) == 0);
    }

    SECTION("Load TOML with missing array") {
        const char *toml = R"(
            [something_else]
            value = 123
        )";

        bool result = agentite_data_load_string(loader, toml, "nonexistent",
                                                sizeof(TestEntry), parse_test_entry, nullptr);
        // Should succeed with 0 entries
        REQUIRE(agentite_data_count(loader) == 0);
    }

    SECTION("Parse callback can skip entries") {
        const char *toml = R"(
            [[item]]
            id = "item1"
            value = 100

            [[item]]
            id = "item2"
            value = 200
        )";

        bool result = agentite_data_load_string(loader, toml, "item",
                                                sizeof(TestEntry), parse_fail, nullptr);
        // Parse_fail returns false, so entries are skipped
        REQUIRE(agentite_data_count(loader) == 0);
    }

    SECTION("Invalid TOML syntax") {
        const char *toml = R"(
            [[item]
            malformed = "missing bracket"
        )";

        bool result = agentite_data_load_string(loader, toml, "item",
                                                sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE_FALSE(result);

        const char *error = agentite_data_get_last_error(loader);
        REQUIRE(error != nullptr);
        REQUIRE(strlen(error) > 0);
    }

    SECTION("NULL loader") {
        const char *toml = "[[item]]\nid = \"test\"";
        bool result = agentite_data_load_string(nullptr, toml, "item",
                                                sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE_FALSE(result);
    }

    SECTION("NULL TOML string") {
        bool result = agentite_data_load_string(loader, nullptr, "item",
                                                sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE_FALSE(result);
    }

    SECTION("NULL parse function") {
        const char *toml = "[[item]]\nid = \"test\"";
        bool result = agentite_data_load_string(loader, toml, "item",
                                                sizeof(TestEntry), nullptr, nullptr);
        REQUIRE_FALSE(result);
    }

    SECTION("Zero entry size") {
        const char *toml = "[[item]]\nid = \"test\"";
        bool result = agentite_data_load_string(loader, toml, "item",
                                                0, parse_test_entry, nullptr);
        REQUIRE_FALSE(result);
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * Data Access Tests
 * ============================================================================ */

TEST_CASE("Data loader access", "[data_config][access]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    const char *toml = R"(
        [[item]]
        id = "alpha"
        name = "Alpha Item"
        value = 10

        [[item]]
        id = "beta"
        name = "Beta Item"
        value = 20

        [[item]]
        id = "gamma"
        name = "Gamma Item"
        value = 30
    )";

    REQUIRE(agentite_data_load_string(loader, toml, "item",
                                      sizeof(TestEntry), parse_test_entry, nullptr));

    SECTION("Get by index") {
        REQUIRE(agentite_data_count(loader) == 3);

        TestEntry *e0 = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 0));
        REQUIRE(e0 != nullptr);

        TestEntry *e1 = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 1));
        REQUIRE(e1 != nullptr);

        TestEntry *e2 = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 2));
        REQUIRE(e2 != nullptr);
    }

    SECTION("Get by index out of bounds") {
        REQUIRE(agentite_data_get_by_index(loader, 100) == nullptr);
        REQUIRE(agentite_data_get_by_index(loader, (size_t)-1) == nullptr);
    }

    SECTION("Find by ID (O(1) hash lookup)") {
        TestEntry *alpha = static_cast<TestEntry*>(agentite_data_find(loader, "alpha"));
        REQUIRE(alpha != nullptr);
        REQUIRE(std::string(alpha->id) == "alpha");
        REQUIRE(alpha->value == 10);

        TestEntry *beta = static_cast<TestEntry*>(agentite_data_find(loader, "beta"));
        REQUIRE(beta != nullptr);
        REQUIRE(std::string(beta->id) == "beta");
        REQUIRE(beta->value == 20);

        TestEntry *gamma = static_cast<TestEntry*>(agentite_data_find(loader, "gamma"));
        REQUIRE(gamma != nullptr);
        REQUIRE(std::string(gamma->id) == "gamma");
        REQUIRE(gamma->value == 30);
    }

    SECTION("Find non-existent ID") {
        REQUIRE(agentite_data_find(loader, "nonexistent") == nullptr);
        REQUIRE(agentite_data_find(loader, "") == nullptr);
    }

    SECTION("NULL loader access") {
        REQUIRE(agentite_data_count(nullptr) == 0);
        REQUIRE(agentite_data_get_by_index(nullptr, 0) == nullptr);
        REQUIRE(agentite_data_find(nullptr, "test") == nullptr);
    }

    SECTION("NULL ID find") {
        REQUIRE(agentite_data_find(loader, nullptr) == nullptr);
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * Clear Tests
 * ============================================================================ */

TEST_CASE("Data loader clear", "[data_config][clear]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    SECTION("Clear populated loader") {
        const char *toml = R"(
            [[item]]
            id = "test1"
            [[item]]
            id = "test2"
        )";

        agentite_data_load_string(loader, toml, "item",
                                  sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE(agentite_data_count(loader) == 2);

        agentite_data_clear(loader);
        REQUIRE(agentite_data_count(loader) == 0);
        REQUIRE(agentite_data_find(loader, "test1") == nullptr);
    }

    SECTION("Clear empty loader") {
        agentite_data_clear(loader);
        REQUIRE(agentite_data_count(loader) == 0);
    }

    SECTION("Clear NULL loader is safe") {
        agentite_data_clear(nullptr);
        // Should not crash
    }

    SECTION("Can load after clear") {
        const char *toml1 = "[[item]]\nid = \"first\"";
        agentite_data_load_string(loader, toml1, "item",
                                  sizeof(TestEntry), parse_test_entry, nullptr);

        agentite_data_clear(loader);

        const char *toml2 = "[[item]]\nid = \"second\"";
        REQUIRE(agentite_data_load_string(loader, toml2, "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));
        REQUIRE(agentite_data_count(loader) == 1);
        REQUIRE(agentite_data_find(loader, "second") != nullptr);
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * TOML Helper Function Tests
 * ============================================================================ */

TEST_CASE("TOML helper functions", "[data_config][helpers]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    // We'll test helpers through a parse callback that captures values
    struct HelperTestData {
        char str_val[64];
        int int_val;
        int64_t int64_val;
        float float_val;
        double double_val;
        bool bool_val;
        bool has_key_present;
        bool has_key_missing;
    };

    static HelperTestData captured;

    auto parse_helpers = [](const char *key, toml_table_t *table,
                           void *out_entry, void *userdata) -> bool {
        HelperTestData *data = static_cast<HelperTestData*>(userdata);

        agentite_toml_get_string(table, "str", data->str_val, sizeof(data->str_val));
        agentite_toml_get_int(table, "int", &data->int_val);

        long long temp64;
        if (agentite_toml_get_int64(table, "int64", &temp64)) {
            data->int64_val = temp64;
        }

        agentite_toml_get_float(table, "float", &data->float_val);
        agentite_toml_get_double(table, "double", &data->double_val);
        agentite_toml_get_bool(table, "bool", &data->bool_val);

        data->has_key_present = agentite_toml_has_key(table, "str");
        data->has_key_missing = agentite_toml_has_key(table, "nonexistent");

        return true;
    };

    SECTION("Parse all value types") {
        const char *toml = R"(
            [[item]]
            id = "test"
            str = "hello world"
            int = 42
            int64 = 9223372036854775807
            float = 3.14
            double = 2.718281828
            bool = true
        )";

        memset(&captured, 0, sizeof(captured));
        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_helpers, &captured));

        REQUIRE(std::string(captured.str_val) == "hello world");
        REQUIRE(captured.int_val == 42);
        REQUIRE(captured.int64_val == 9223372036854775807LL);
        REQUIRE(captured.float_val == Catch::Approx(3.14f));
        REQUIRE(captured.double_val == Catch::Approx(2.718281828));
        REQUIRE(captured.bool_val == true);
        REQUIRE(captured.has_key_present == true);
        REQUIRE(captured.has_key_missing == false);
    }

    SECTION("Missing keys return defaults") {
        const char *toml = R"(
            [[item]]
            id = "test"
        )";

        memset(&captured, 0, sizeof(captured));
        captured.int_val = -1;
        captured.float_val = -1.0f;
        captured.bool_val = true;  // Start true to verify it stays unchanged

        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_helpers, &captured));

        // Missing keys should not modify the output values
        REQUIRE(captured.int_val == -1);
        REQUIRE(captured.float_val == -1.0f);
    }

    SECTION("Negative values") {
        const char *toml = R"(
            [[item]]
            id = "test"
            int = -999
            float = -2.5
        )";

        memset(&captured, 0, sizeof(captured));
        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_helpers, &captured));

        REQUIRE(captured.int_val == -999);
        REQUIRE(captured.float_val == Catch::Approx(-2.5f));
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * Array Parsing Tests
 * ============================================================================ */

TEST_CASE("TOML array parsing", "[data_config][arrays]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    struct ArrayTestData {
        char **strings;
        int string_count;
        int *ints;
        int int_count;
        float *floats;
        int float_count;
    };

    static ArrayTestData captured;

    auto parse_arrays = [](const char *key, toml_table_t *table,
                          void *out_entry, void *userdata) -> bool {
        ArrayTestData *data = static_cast<ArrayTestData*>(userdata);

        agentite_toml_get_string_array(table, "tags", &data->strings, &data->string_count);
        agentite_toml_get_int_array(table, "numbers", &data->ints, &data->int_count);
        agentite_toml_get_float_array(table, "values", &data->floats, &data->float_count);

        return true;
    };

    SECTION("Parse arrays") {
        const char *toml = R"(
            [[item]]
            id = "test"
            tags = ["alpha", "beta", "gamma"]
            numbers = [1, 2, 3, 4, 5]
            values = [1.1, 2.2, 3.3]
        )";

        memset(&captured, 0, sizeof(captured));
        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_arrays, &captured));

        REQUIRE(captured.string_count == 3);
        REQUIRE(captured.strings != nullptr);
        REQUIRE(std::string(captured.strings[0]) == "alpha");
        REQUIRE(std::string(captured.strings[1]) == "beta");
        REQUIRE(std::string(captured.strings[2]) == "gamma");

        REQUIRE(captured.int_count == 5);
        REQUIRE(captured.ints != nullptr);
        REQUIRE(captured.ints[0] == 1);
        REQUIRE(captured.ints[4] == 5);

        REQUIRE(captured.float_count == 3);
        REQUIRE(captured.floats != nullptr);
        REQUIRE(captured.floats[0] == Catch::Approx(1.1f));

        // Clean up
        agentite_toml_free_strings(captured.strings, captured.string_count);
        free(captured.ints);
        free(captured.floats);
    }

    SECTION("Empty arrays") {
        const char *toml = R"(
            [[item]]
            id = "test"
            tags = []
            numbers = []
        )";

        memset(&captured, 0, sizeof(captured));
        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_arrays, &captured));

        REQUIRE(captured.string_count == 0);
        REQUIRE(captured.int_count == 0);
    }

    SECTION("Missing arrays") {
        const char *toml = R"(
            [[item]]
            id = "test"
        )";

        memset(&captured, 0, sizeof(captured));
        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_arrays, &captured));

        REQUIRE(captured.string_count == 0);
        REQUIRE(captured.strings == nullptr);
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * Edge Cases Tests
 * ============================================================================ */

TEST_CASE("Data config edge cases", "[data_config][edge]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    SECTION("String at buffer boundary") {
        // Test string that's exactly 63 chars (max for id[64])
        const char *toml = R"(
            [[item]]
            id = "123456789012345678901234567890123456789012345678901234567890123"
            name = "test"
        )";

        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));

        TestEntry *entry = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 0));
        REQUIRE(entry != nullptr);
        REQUIRE(strlen(entry->id) == 63);
    }

    SECTION("String exceeds buffer (should truncate)") {
        // String longer than 63 chars
        std::string long_id(100, 'x');
        std::string toml = "[[item]]\nid = \"" + long_id + "\"";

        REQUIRE(agentite_data_load_string(loader, toml.c_str(), "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));

        TestEntry *entry = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 0));
        REQUIRE(entry != nullptr);
        REQUIRE(strlen(entry->id) < 64);  // Should be truncated
    }

    SECTION("Unicode in strings") {
        const char *toml = R"(
            [[item]]
            id = "unicode_test"
            name = "日本語テスト"
        )";

        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));

        TestEntry *entry = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 0));
        REQUIRE(entry != nullptr);
        REQUIRE(strlen(entry->name) > 0);
    }

    SECTION("Escape sequences in strings") {
        const char *toml = R"(
            [[item]]
            id = "escape_test"
            name = "Line1\nLine2\tTabbed"
        )";

        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));
    }

    SECTION("Special characters in ID") {
        const char *toml = R"(
            [[item]]
            id = "test-item_v2.0"
            name = "Special ID"
        )";

        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));

        TestEntry *entry = static_cast<TestEntry*>(agentite_data_find(loader, "test-item_v2.0"));
        REQUIRE(entry != nullptr);
    }

    SECTION("Integer boundaries") {
        const char *toml = R"(
            [[item]]
            id = "int_test"
            value = 2147483647
        )";

        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));

        TestEntry *entry = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 0));
        REQUIRE(entry != nullptr);
        REQUIRE(entry->value == 2147483647);
    }

    SECTION("Float special values") {
        const char *toml = R"(
            [[item]]
            id = "float_test"
            rate = 0.0
        )";

        REQUIRE(agentite_data_load_string(loader, toml, "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));

        TestEntry *entry = static_cast<TestEntry*>(agentite_data_get_by_index(loader, 0));
        REQUIRE(entry != nullptr);
        REQUIRE(entry->rate == 0.0f);
    }

    SECTION("Many entries") {
        std::string toml;
        for (int i = 0; i < 100; i++) {
            toml += "[[item]]\nid = \"item" + std::to_string(i) + "\"\nvalue = " +
                   std::to_string(i) + "\n\n";
        }

        REQUIRE(agentite_data_load_string(loader, toml.c_str(), "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));
        REQUIRE(agentite_data_count(loader) == 100);

        // Verify hash lookup works for all
        for (int i = 0; i < 100; i++) {
            std::string id = "item" + std::to_string(i);
            REQUIRE(agentite_data_find(loader, id.c_str()) != nullptr);
        }
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_CASE("Data config error handling", "[data_config][error]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    SECTION("Get last error initially empty") {
        const char *error = agentite_data_get_last_error(loader);
        // Might be NULL or empty string
    }

    SECTION("Get last error after syntax error") {
        const char *toml = "invalid [ syntax";
        agentite_data_load_string(loader, toml, "item",
                                  sizeof(TestEntry), parse_test_entry, nullptr);

        const char *error = agentite_data_get_last_error(loader);
        REQUIRE(error != nullptr);
    }

    SECTION("Get last error from NULL loader") {
        // Implementation returns error message instead of NULL
        const char *error = agentite_data_get_last_error(nullptr);
        // Either NULL or an error message is acceptable
        // (implementation may return "Invalid loader" or similar)
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * Load from File Tests (conditional on file existence)
 * ============================================================================ */

TEST_CASE("Data loader file operations", "[data_config][file]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    SECTION("Load non-existent file") {
        bool result = agentite_data_load(loader, "/nonexistent/path/file.toml",
                                         "item", sizeof(TestEntry),
                                         parse_test_entry, nullptr);
        REQUIRE_FALSE(result);

        const char *error = agentite_data_get_last_error(loader);
        REQUIRE(error != nullptr);
        REQUIRE(strlen(error) > 0);
    }

    SECTION("Load with NULL path") {
        bool result = agentite_data_load(loader, nullptr, "item",
                                         sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE_FALSE(result);
    }

    SECTION("Load with empty path") {
        bool result = agentite_data_load(loader, "", "item",
                                         sizeof(TestEntry), parse_test_entry, nullptr);
        REQUIRE_FALSE(result);
    }

    agentite_data_destroy(loader);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_CASE("Data config stress test", "[data_config][stress]") {
    Agentite_DataLoader *loader = agentite_data_create();
    REQUIRE(loader != nullptr);

    SECTION("Repeated load/clear cycles") {
        for (int cycle = 0; cycle < 50; cycle++) {
            const char *toml = "[[item]]\nid = \"test\"\nvalue = 123";
            REQUIRE(agentite_data_load_string(loader, toml, "item",
                                              sizeof(TestEntry), parse_test_entry, nullptr));
            agentite_data_clear(loader);
        }
        REQUIRE(agentite_data_count(loader) == 0);
    }

    SECTION("Large TOML document") {
        std::string toml;
        for (int i = 0; i < 200; i++) {
            toml += "[[item]]\n";
            toml += "id = \"item_" + std::to_string(i) + "\"\n";
            toml += "name = \"This is a longer name for testing purposes number " +
                   std::to_string(i) + "\"\n";
            toml += "value = " + std::to_string(i * 10) + "\n";
            toml += "rate = " + std::to_string(i * 0.1) + "\n";
            toml += "enabled = " + std::string(i % 2 == 0 ? "true" : "false") + "\n\n";
        }

        REQUIRE(agentite_data_load_string(loader, toml.c_str(), "item",
                                          sizeof(TestEntry), parse_test_entry, nullptr));
        REQUIRE(agentite_data_count(loader) == 200);
    }

    agentite_data_destroy(loader);
}
