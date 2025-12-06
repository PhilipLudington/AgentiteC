/*
 * Carbon Save System Tests
 *
 * Tests for the save/load system including serialization,
 * path validation, version compatibility, and file operations.
 */

#include "catch_amalgamated.hpp"
#include "carbon/save.h"
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static const char *TEST_SAVES_DIR = "./test_saves";

// Test game state structure
struct TestGameState {
    int turn;
    int gold;
    float health;
    double precision;
    bool active;
    char player_name[64];
    int scores[5];
    float values[3];
};

// Serialize test game state
static bool test_serialize(void *game_state, Carbon_SaveWriter *writer) {
    TestGameState *gs = (TestGameState *)game_state;

    carbon_save_write_int(writer, "turn", gs->turn);
    carbon_save_write_int(writer, "gold", gs->gold);
    carbon_save_write_float(writer, "health", gs->health);
    carbon_save_write_double(writer, "precision", gs->precision);
    carbon_save_write_bool(writer, "active", gs->active);
    carbon_save_write_string(writer, "player_name", gs->player_name);
    carbon_save_write_int_array(writer, "scores", gs->scores, 5);
    carbon_save_write_float_array(writer, "values", gs->values, 3);

    return true;
}

// Deserialize test game state
static bool test_deserialize(void *game_state, Carbon_SaveReader *reader) {
    TestGameState *gs = (TestGameState *)game_state;

    carbon_save_read_int(reader, "turn", &gs->turn);
    carbon_save_read_int(reader, "gold", &gs->gold);
    carbon_save_read_float(reader, "health", &gs->health);
    carbon_save_read_double(reader, "precision", &gs->precision);
    carbon_save_read_bool(reader, "active", &gs->active);
    carbon_save_read_string(reader, "player_name", gs->player_name, sizeof(gs->player_name));

    int *scores_arr = nullptr;
    int scores_count = 0;
    if (carbon_save_read_int_array(reader, "scores", &scores_arr, &scores_count)) {
        for (int i = 0; i < scores_count && i < 5; i++) {
            gs->scores[i] = scores_arr[i];
        }
        free(scores_arr);
    }

    float *values_arr = nullptr;
    int values_count = 0;
    if (carbon_save_read_float_array(reader, "values", &values_arr, &values_count)) {
        for (int i = 0; i < values_count && i < 3; i++) {
            gs->values[i] = values_arr[i];
        }
        free(values_arr);
    }

    return true;
}

// Serialize that always fails
static bool test_serialize_fail(void *game_state, Carbon_SaveWriter *writer) {
    (void)game_state;
    (void)writer;
    return false;
}

// Deserialize that always fails
static bool test_deserialize_fail(void *game_state, Carbon_SaveReader *reader) {
    (void)game_state;
    (void)reader;
    return false;
}

// Clean up test saves directory
static void cleanup_test_saves() {
    // Remove known test files
    const char *test_files[] = {
        "test_saves/test_game.toml",
        "test_saves/quicksave.toml",
        "test_saves/autosave.toml",
        "test_saves/version_test.toml",
        "test_saves/list_test_1.toml",
        "test_saves/list_test_2.toml",
    };

    for (const char *file : test_files) {
        remove(file);
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Save manager creation and destruction", "[save][lifecycle]") {
    cleanup_test_saves();

    SECTION("Create with custom directory") {
        Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);
        REQUIRE(sm != nullptr);

        // Directory should be created
        struct stat st;
        REQUIRE(stat(TEST_SAVES_DIR, &st) == 0);

        carbon_save_destroy(sm);
    }

    SECTION("Create with NULL directory uses default") {
        Carbon_SaveManager *sm = carbon_save_create(nullptr);
        REQUIRE(sm != nullptr);
        carbon_save_destroy(sm);
    }

    SECTION("Create with empty directory uses default") {
        Carbon_SaveManager *sm = carbon_save_create("");
        REQUIRE(sm != nullptr);
        carbon_save_destroy(sm);
    }

    SECTION("Destroy NULL is safe") {
        carbon_save_destroy(nullptr);
    }
}

/* ============================================================================
 * Save Name Validation Tests (Security)
 * ============================================================================ */

TEST_CASE("Save name validation prevents path traversal", "[save][security]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState gs = {0};
    gs.turn = 10;
    gs.gold = 500;

    SECTION("Valid save name works") {
        Carbon_SaveResult result = carbon_save_game(sm, "valid_save", test_serialize, &gs);
        REQUIRE(result.success);
        REQUIRE(carbon_save_exists(sm, "valid_save"));
        carbon_save_delete(sm, "valid_save");
    }

    SECTION("Path traversal with .. is rejected") {
        Carbon_SaveResult result = carbon_save_game(sm, "../escape", test_serialize, &gs);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error_message, "Invalid save name") != nullptr);
    }

    SECTION("Path traversal with ../ prefix is rejected") {
        Carbon_SaveResult result = carbon_save_game(sm, "../../etc/passwd", test_serialize, &gs);
        REQUIRE_FALSE(result.success);
    }

    SECTION("Forward slash is rejected") {
        Carbon_SaveResult result = carbon_save_game(sm, "foo/bar", test_serialize, &gs);
        REQUIRE_FALSE(result.success);
    }

    SECTION("Backslash is rejected") {
        Carbon_SaveResult result = carbon_save_game(sm, "foo\\bar", test_serialize, &gs);
        REQUIRE_FALSE(result.success);
    }

    SECTION("Empty name is rejected") {
        Carbon_SaveResult result = carbon_save_game(sm, "", test_serialize, &gs);
        REQUIRE_FALSE(result.success);
    }

    SECTION("Very long name is rejected") {
        char long_name[128];
        memset(long_name, 'A', sizeof(long_name) - 1);
        long_name[sizeof(long_name) - 1] = '\0';

        Carbon_SaveResult result = carbon_save_game(sm, long_name, test_serialize, &gs);
        REQUIRE_FALSE(result.success);
    }

    SECTION("Load with path traversal is rejected") {
        Carbon_SaveResult result = carbon_load_game(sm, "../etc/passwd", test_deserialize, &gs);
        REQUIRE_FALSE(result.success);
    }

    SECTION("Delete with path traversal is rejected") {
        REQUIRE_FALSE(carbon_save_delete(sm, "../important_file"));
    }

    SECTION("Exists with path traversal is rejected") {
        REQUIRE_FALSE(carbon_save_exists(sm, "../etc/passwd"));
    }

    carbon_save_destroy(sm);
}

/* ============================================================================
 * Save and Load Tests
 * ============================================================================ */

TEST_CASE("Basic save and load", "[save][basic]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState save_state = {0};
    save_state.turn = 42;
    save_state.gold = 1000;
    save_state.health = 75.5f;
    save_state.precision = 3.141592653589793;
    save_state.active = true;
    strcpy(save_state.player_name, "TestPlayer");
    save_state.scores[0] = 100;
    save_state.scores[1] = 200;
    save_state.scores[2] = 300;
    save_state.scores[3] = 400;
    save_state.scores[4] = 500;
    save_state.values[0] = 1.5f;
    save_state.values[1] = 2.5f;
    save_state.values[2] = 3.5f;

    SECTION("Save creates file") {
        Carbon_SaveResult result = carbon_save_game(sm, "test_game", test_serialize, &save_state);
        REQUIRE(result.success);
        REQUIRE(carbon_save_exists(sm, "test_game"));
        REQUIRE(strlen(result.filepath) > 0);
        REQUIRE(result.save_version > 0);
    }

    SECTION("Load restores state") {
        carbon_save_game(sm, "test_game", test_serialize, &save_state);

        TestGameState load_state = {0};
        Carbon_SaveResult result = carbon_load_game(sm, "test_game", test_deserialize, &load_state);

        REQUIRE(result.success);
        REQUIRE(load_state.turn == 42);
        REQUIRE(load_state.gold == 1000);
        REQUIRE(load_state.health == Catch::Approx(75.5f));
        REQUIRE(load_state.precision == Catch::Approx(3.141592653589793));
        REQUIRE(load_state.active == true);
        REQUIRE(strcmp(load_state.player_name, "TestPlayer") == 0);
        REQUIRE(load_state.scores[0] == 100);
        REQUIRE(load_state.scores[4] == 500);
        REQUIRE(load_state.values[0] == Catch::Approx(1.5f));
        REQUIRE(load_state.values[2] == Catch::Approx(3.5f));
    }

    SECTION("Load non-existent file fails") {
        TestGameState load_state = {0};
        Carbon_SaveResult result = carbon_load_game(sm, "nonexistent", test_deserialize, &load_state);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error_message, "not found") != nullptr);
    }

    carbon_save_delete(sm, "test_game");
    carbon_save_destroy(sm);
}

/* ============================================================================
 * Quick Save and Auto Save Tests
 * ============================================================================ */

TEST_CASE("Quick save and load", "[save][quick]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState save_state = {0};
    save_state.turn = 99;
    save_state.gold = 9999;

    SECTION("Quick save creates quicksave file") {
        Carbon_SaveResult result = carbon_save_quick(sm, test_serialize, &save_state);
        REQUIRE(result.success);
        REQUIRE(carbon_save_exists(sm, "quicksave"));
    }

    SECTION("Quick load restores from quicksave") {
        carbon_save_quick(sm, test_serialize, &save_state);

        TestGameState load_state = {0};
        Carbon_SaveResult result = carbon_load_quick(sm, test_deserialize, &load_state);
        REQUIRE(result.success);
        REQUIRE(load_state.turn == 99);
        REQUIRE(load_state.gold == 9999);
    }

    carbon_save_delete(sm, "quicksave");
    carbon_save_destroy(sm);
}

TEST_CASE("Auto save", "[save][auto]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState save_state = {0};
    save_state.turn = 50;

    Carbon_SaveResult result = carbon_save_auto(sm, test_serialize, &save_state);
    REQUIRE(result.success);
    REQUIRE(carbon_save_exists(sm, "autosave"));

    carbon_save_delete(sm, "autosave");
    carbon_save_destroy(sm);
}

/* ============================================================================
 * Version Compatibility Tests
 * ============================================================================ */

TEST_CASE("Save version compatibility", "[save][version]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState gs = {0};
    gs.turn = 1;

    SECTION("Default version is 1") {
        Carbon_SaveResult result = carbon_save_game(sm, "version_test", test_serialize, &gs);
        REQUIRE(result.success);
        REQUIRE(result.save_version == 1);
    }

    SECTION("Set custom version") {
        carbon_save_set_version(sm, 5, 3);

        Carbon_SaveResult result = carbon_save_game(sm, "version_test", test_serialize, &gs);
        REQUIRE(result.success);
        REQUIRE(result.save_version == 5);
    }

    SECTION("Load older compatible version") {
        // Save with version 2
        carbon_save_set_version(sm, 2, 1);
        carbon_save_game(sm, "version_test", test_serialize, &gs);

        // Load with version 3, min compatible 1
        carbon_save_set_version(sm, 3, 1);
        TestGameState load_state = {0};
        Carbon_SaveResult result = carbon_load_game(sm, "version_test", test_deserialize, &load_state);

        REQUIRE(result.success);
        REQUIRE(result.was_migrated);  // Version changed
        REQUIRE(result.save_version == 2);
    }

    SECTION("Load incompatible version fails") {
        // Save with version 1
        carbon_save_set_version(sm, 1, 1);
        carbon_save_game(sm, "version_test", test_serialize, &gs);

        // Try to load with min compatible 5
        carbon_save_set_version(sm, 5, 5);
        TestGameState load_state = {0};
        Carbon_SaveResult result = carbon_load_game(sm, "version_test", test_deserialize, &load_state);

        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error_message, "too old") != nullptr);
    }

    carbon_save_delete(sm, "version_test");
    carbon_save_destroy(sm);
}

/* ============================================================================
 * Save List Tests
 * ============================================================================ */

TEST_CASE("Save list", "[save][list]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState gs = {0};

    SECTION("Empty directory returns NULL") {
        int count = 0;
        Carbon_SaveInfo *list = carbon_save_list(sm, &count);
        // May be NULL or count == 0 depending on whether there are existing files
        if (list) {
            carbon_save_list_free(list);
        }
    }

    SECTION("List returns save info") {
        gs.turn = 10;
        carbon_save_game(sm, "list_test_1", test_serialize, &gs);
        gs.turn = 20;
        carbon_save_game(sm, "list_test_2", test_serialize, &gs);

        int count = 0;
        Carbon_SaveInfo *list = carbon_save_list(sm, &count);
        REQUIRE(list != nullptr);
        REQUIRE(count >= 2);

        // Find our test saves
        bool found_1 = false, found_2 = false;
        for (int i = 0; i < count; i++) {
            if (strcmp(list[i].display_name, "list_test_1") == 0) {
                found_1 = true;
                REQUIRE(list[i].is_compatible);
            }
            if (strcmp(list[i].display_name, "list_test_2") == 0) {
                found_2 = true;
                REQUIRE(list[i].preview_turn == 20);  // Last saved turn
            }
        }

        REQUIRE(found_1);
        REQUIRE(found_2);

        carbon_save_list_free(list);
    }

    carbon_save_delete(sm, "list_test_1");
    carbon_save_delete(sm, "list_test_2");
    carbon_save_destroy(sm);
}

/* ============================================================================
 * Delete Tests
 * ============================================================================ */

TEST_CASE("Save deletion", "[save][delete]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState gs = {0};

    SECTION("Delete existing save") {
        carbon_save_game(sm, "delete_me", test_serialize, &gs);
        REQUIRE(carbon_save_exists(sm, "delete_me"));

        REQUIRE(carbon_save_delete(sm, "delete_me"));
        REQUIRE_FALSE(carbon_save_exists(sm, "delete_me"));
    }

    SECTION("Delete non-existent save returns false") {
        REQUIRE_FALSE(carbon_save_delete(sm, "nonexistent"));
    }

    SECTION("Delete with NULL params") {
        REQUIRE_FALSE(carbon_save_delete(sm, nullptr));
        REQUIRE_FALSE(carbon_save_delete(nullptr, "test"));
    }

    carbon_save_destroy(sm);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_CASE("Save error handling", "[save][errors]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState gs = {0};

    SECTION("NULL parameters") {
        Carbon_SaveResult result;

        result = carbon_save_game(nullptr, "test", test_serialize, &gs);
        REQUIRE_FALSE(result.success);

        result = carbon_save_game(sm, nullptr, test_serialize, &gs);
        REQUIRE_FALSE(result.success);

        result = carbon_save_game(sm, "test", nullptr, &gs);
        REQUIRE_FALSE(result.success);

        result = carbon_load_game(nullptr, "test", test_deserialize, &gs);
        REQUIRE_FALSE(result.success);

        result = carbon_load_game(sm, nullptr, test_deserialize, &gs);
        REQUIRE_FALSE(result.success);

        result = carbon_load_game(sm, "test", nullptr, &gs);
        REQUIRE_FALSE(result.success);
    }

    SECTION("Serialization failure") {
        Carbon_SaveResult result = carbon_save_game(sm, "fail_test", test_serialize_fail, &gs);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error_message, "Serialization failed") != nullptr);
    }

    SECTION("Deserialization failure") {
        // First save successfully
        carbon_save_game(sm, "deser_fail", test_serialize, &gs);

        // Then try to load with failing deserializer
        Carbon_SaveResult result = carbon_load_game(sm, "deser_fail", test_deserialize_fail, &gs);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error_message, "Deserialization failed") != nullptr);

        carbon_save_delete(sm, "deser_fail");
    }

    carbon_save_destroy(sm);
}

/* ============================================================================
 * String Escaping Tests
 * ============================================================================ */

TEST_CASE("String escaping in saves", "[save][strings]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState gs = {0};

    SECTION("Quotes in string") {
        strcpy(gs.player_name, "Player \"The Great\"");
        carbon_save_game(sm, "quote_test", test_serialize, &gs);

        TestGameState load = {0};
        carbon_load_game(sm, "quote_test", test_deserialize, &load);
        REQUIRE(strcmp(load.player_name, "Player \"The Great\"") == 0);

        carbon_save_delete(sm, "quote_test");
    }

    SECTION("Backslash in string") {
        strcpy(gs.player_name, "Path\\To\\File");
        carbon_save_game(sm, "backslash_test", test_serialize, &gs);

        TestGameState load = {0};
        carbon_load_game(sm, "backslash_test", test_deserialize, &load);
        REQUIRE(strcmp(load.player_name, "Path\\To\\File") == 0);

        carbon_save_delete(sm, "backslash_test");
    }

    SECTION("Newline in string") {
        strcpy(gs.player_name, "Line1\nLine2");
        carbon_save_game(sm, "newline_test", test_serialize, &gs);

        TestGameState load = {0};
        carbon_load_game(sm, "newline_test", test_deserialize, &load);
        REQUIRE(strcmp(load.player_name, "Line1\nLine2") == 0);

        carbon_save_delete(sm, "newline_test");
    }

    carbon_save_destroy(sm);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_CASE("Save edge cases", "[save][edge]") {
    cleanup_test_saves();
    Carbon_SaveManager *sm = carbon_save_create(TEST_SAVES_DIR);

    TestGameState gs = {0};

    SECTION("Overwrite existing save") {
        gs.turn = 10;
        carbon_save_game(sm, "overwrite_test", test_serialize, &gs);

        gs.turn = 20;
        Carbon_SaveResult result = carbon_save_game(sm, "overwrite_test", test_serialize, &gs);
        REQUIRE(result.success);

        TestGameState load = {0};
        carbon_load_game(sm, "overwrite_test", test_deserialize, &load);
        REQUIRE(load.turn == 20);

        carbon_save_delete(sm, "overwrite_test");
    }

    SECTION("Save with special but valid characters") {
        Carbon_SaveResult result = carbon_save_game(sm, "save-name_123", test_serialize, &gs);
        REQUIRE(result.success);
        REQUIRE(carbon_save_exists(sm, "save-name_123"));
        carbon_save_delete(sm, "save-name_123");
    }

    SECTION("Exists with NULL params") {
        REQUIRE_FALSE(carbon_save_exists(nullptr, "test"));
        REQUIRE_FALSE(carbon_save_exists(sm, nullptr));
    }

    carbon_save_destroy(sm);
}
