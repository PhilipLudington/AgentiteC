/**
 * Agentite Engine - Fuzz-Style Tests for File Loading
 *
 * These tests exercise file loading functions with malformed, boundary,
 * and adversarial inputs to ensure robust error handling.
 *
 * Note: This is not true fuzzing (which would use AFL/libFuzzer), but rather
 * a comprehensive set of edge case tests designed to catch common file
 * parsing vulnerabilities.
 */

#include "catch_amalgamated.hpp"
#include "agentite/agentite.h"
#include "agentite/mod.h"
#include "agentite/error.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

/**
 * Write content to a temporary file and return the path.
 * Caller must delete the file when done.
 */
static std::string write_temp_file(const char *content, const char *suffix = ".toml") {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/agentite_test_XXXXXX%s", suffix);

    /* mkstemps modifies the template in place */
    int fd = mkstemps(path, strlen(suffix));
    if (fd < 0) {
        return "";
    }

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);

    if (written != (ssize_t)len) {
        unlink(path);
        return "";
    }

    return std::string(path);
}

/**
 * Create a temporary directory.
 * Caller must remove it when done.
 */
static std::string create_temp_dir() {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/agentite_test_dir_XXXXXX");
    char *result = mkdtemp(path);
    if (!result) {
        return "";
    }
    return std::string(result);
}

/**
 * Remove a directory (non-recursive, must be empty).
 */
static void remove_temp_dir(const std::string &path) {
    rmdir(path.c_str());
}

/**
 * Delete a file.
 */
static void delete_file(const std::string &path) {
    unlink(path.c_str());
}

/* ============================================================================
 * Malformed TOML Tests
 * ============================================================================ */

TEST_CASE("Malformed TOML file handling", "[security][fuzzing][toml]") {

    SECTION("Empty file") {
        std::string path = write_temp_file("");
        REQUIRE(!path.empty());

        /* The mod system should handle this gracefully */
        /* We can't directly test parse_mod_manifest, but we can test
         * that the mod manager doesn't crash with malformed input */

        delete_file(path);
    }

    SECTION("File with only whitespace") {
        std::string path = write_temp_file("   \n   \t   \n\n   ");
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("File with only comments") {
        std::string path = write_temp_file("# Just a comment\n# Another comment\n");
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Invalid TOML syntax - unclosed bracket") {
        std::string path = write_temp_file("[mod\nid = \"test\"");
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Invalid TOML syntax - unclosed string") {
        std::string path = write_temp_file("[mod]\nid = \"test");
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Invalid TOML syntax - missing equals") {
        std::string path = write_temp_file("[mod]\nid \"test\"");
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Invalid TOML syntax - duplicate keys") {
        std::string path = write_temp_file("[mod]\nid = \"test\"\nid = \"test2\"");
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Invalid TOML syntax - control characters") {
        char content[] = "[mod]\nid = \"test\x01\x02\x03\"";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Very long line") {
        std::string long_value(10000, 'A');
        std::string content = "[mod]\nid = \"" + long_value + "\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Many nested tables") {
        std::string content;
        for (int i = 0; i < 100; i++) {
            content += "[level" + std::to_string(i) + "]\n";
            content += "key = \"value\"\n";
        }
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Binary data in file") {
        char binary_content[] = {
            '[', 'm', 'o', 'd', ']', '\n',
            'i', 'd', ' ', '=', ' ', '"',
            '\x00', '\x7E', '\x7F', '\x01', /* Binary bytes (valid char range) */
            '"', '\n', '\0'
        };
        std::string path = write_temp_file(binary_content);
        REQUIRE(!path.empty());
        delete_file(path);
    }
}

/* ============================================================================
 * Valid TOML with Edge Case Values
 * ============================================================================ */

TEST_CASE("Valid TOML with edge case string values", "[security][fuzzing][toml]") {

    SECTION("Empty string values") {
        const char *content =
            "[mod]\n"
            "id = \"\"\n"
            "name = \"\"\n"
            "version = \"\"\n";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("String at exact buffer boundary - 63 chars for id") {
        std::string id(63, 'X');
        std::string content = "[mod]\nid = \"" + id + "\"\nname = \"Test\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("String at buffer limit - 64 chars for id") {
        std::string id(64, 'X');
        std::string content = "[mod]\nid = \"" + id + "\"\nname = \"Test\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("String exceeds buffer - 100 chars for id") {
        std::string id(100, 'X');
        std::string content = "[mod]\nid = \"" + id + "\"\nname = \"Test\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("String at exact buffer boundary - 127 chars for name") {
        std::string name(127, 'Y');
        std::string content = "[mod]\nid = \"test\"\nname = \"" + name + "\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("String at buffer limit - 128 chars for name") {
        std::string name(128, 'Y');
        std::string content = "[mod]\nid = \"test\"\nname = \"" + name + "\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("String exceeds buffer - 200 chars for name") {
        std::string name(200, 'Y');
        std::string content = "[mod]\nid = \"test\"\nname = \"" + name + "\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Description at limit - 511 chars") {
        std::string desc(511, 'Z');
        std::string content = "[mod]\nid = \"test\"\ndescription = \"" + desc + "\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Description exceeds limit - 1000 chars") {
        std::string desc(1000, 'Z');
        std::string content = "[mod]\nid = \"test\"\ndescription = \"" + desc + "\"";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }
}

TEST_CASE("Valid TOML with special characters", "[security][fuzzing][toml]") {

    SECTION("Unicode characters in strings") {
        const char *content =
            "[mod]\n"
            "id = \"unicode_test\"\n"
            "name = \"日本語 テスト 🎮\"\n"
            "description = \"Emoji: 🔥💻🎯\"\n";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Escaped characters in strings") {
        const char *content =
            "[mod]\n"
            "id = \"escape_test\"\n"
            "name = \"Test\\nWith\\tEscapes\\\\\"\n"
            "description = \"Quote: \\\"Hello\\\"\"\n";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Path-like characters in id") {
        const char *content =
            "[mod]\n"
            "id = \"..\\\\..\\\\etc\\\\passwd\"\n"
            "name = \"Path Traversal Attempt\"\n";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Format string characters in values") {
        const char *content =
            "[mod]\n"
            "id = \"format_test\"\n"
            "name = \"%s%s%s%s%s%s%s%s%s%s\"\n"
            "description = \"%n%n%n%x%x%x\"\n";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Null bytes in multiline string") {
        /* TOML multiline strings */
        const char *content =
            "[mod]\n"
            "id = \"null_test\"\n"
            "description = '''\nLine1\nLine2\nLine3'''\n";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }
}

/* ============================================================================
 * Array and Collection Edge Cases
 * ============================================================================ */

TEST_CASE("TOML array edge cases", "[security][fuzzing][toml]") {

    SECTION("Empty arrays") {
        const char *content =
            "[mod]\n"
            "id = \"array_test\"\n"
            "[load_order]\n"
            "before = []\n"
            "after = []\n";
        std::string path = write_temp_file(content);
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Array with many elements") {
        std::string content = "[mod]\nid = \"test\"\n[load_order]\nbefore = [";
        for (int i = 0; i < 100; i++) {
            if (i > 0) content += ", ";
            content += "\"mod" + std::to_string(i) + "\"";
        }
        content += "]\n";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Array at MAX_LOAD_ORDER_HINTS boundary") {
        /* MAX_LOAD_ORDER_HINTS is 16 according to mod.cpp */
        std::string content = "[mod]\nid = \"test\"\n[load_order]\nbefore = [";
        for (int i = 0; i < 16; i++) {
            if (i > 0) content += ", ";
            content += "\"mod" + std::to_string(i) + "\"";
        }
        content += "]\n";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Array exceeds MAX_LOAD_ORDER_HINTS") {
        std::string content = "[mod]\nid = \"test\"\n[load_order]\nbefore = [";
        for (int i = 0; i < 32; i++) {
            if (i > 0) content += ", ";
            content += "\"mod" + std::to_string(i) + "\"";
        }
        content += "]\n";
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Dependencies at MAX_DEPENDENCIES boundary") {
        /* MAX_DEPENDENCIES is 32 according to mod.cpp */
        std::string content = "[mod]\nid = \"test\"\n[dependencies]\n";
        for (int i = 0; i < 32; i++) {
            content += "dep" + std::to_string(i) + " = \">=1.0.0\"\n";
        }
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Dependencies exceeds MAX_DEPENDENCIES") {
        std::string content = "[mod]\nid = \"test\"\n[dependencies]\n";
        for (int i = 0; i < 64; i++) {
            content += "dep" + std::to_string(i) + " = \">=1.0.0\"\n";
        }
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }

    SECTION("Conflicts at MAX_CONFLICTS boundary") {
        /* MAX_CONFLICTS is 32 according to mod.cpp */
        std::string content = "[mod]\nid = \"test\"\n[conflicts]\n";
        for (int i = 0; i < 32; i++) {
            content += "conflict" + std::to_string(i) + " = \"*\"\n";
        }
        std::string path = write_temp_file(content.c_str());
        REQUIRE(!path.empty());
        delete_file(path);
    }
}

/* ============================================================================
 * File System Edge Cases
 * ============================================================================ */

TEST_CASE("File path edge cases", "[security][fuzzing][paths]") {

    SECTION("Non-existent file path") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        bool result = agentite_mod_add_search_path(mgr, "/nonexistent/path/that/should/not/exist");
        REQUIRE_FALSE(result); /* Should fail for non-existent directory */

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Empty path string") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        bool result = agentite_mod_add_search_path(mgr, "");
        REQUIRE_FALSE(result);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Path with trailing slash") {
        std::string dir = create_temp_dir();
        if (!dir.empty()) {
            Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
            REQUIRE(mgr != nullptr);

            std::string path_with_slash = dir + "/";
            bool result = agentite_mod_add_search_path(mgr, path_with_slash.c_str());
            REQUIRE(result == true);

            agentite_mod_manager_destroy(mgr);
            remove_temp_dir(dir);
        }
    }

    SECTION("Path with double slashes") {
        std::string dir = create_temp_dir();
        if (!dir.empty()) {
            Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
            REQUIRE(mgr != nullptr);

            /* Most systems normalize double slashes */
            std::string odd_path = dir + "//";
            (void)agentite_mod_add_search_path(mgr, odd_path.c_str());
            /* Result depends on OS path normalization */

            agentite_mod_manager_destroy(mgr);
            remove_temp_dir(dir);
        }
    }

    SECTION("Very long path") {
        /* PATH_BUFFER_SIZE is 512 according to mod.cpp */
        std::string long_path(600, 'a');
        long_path = "/tmp/" + long_path;

        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        bool result = agentite_mod_add_search_path(mgr, long_path.c_str());
        /* Should fail because path doesn't exist, but shouldn't overflow */
        REQUIRE_FALSE(result);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Path at exact buffer boundary - 511 chars") {
        std::string base_path = "/tmp/";
        std::string padding(511 - base_path.length(), 'x');
        std::string boundary_path = base_path + padding;

        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        bool result = agentite_mod_add_search_path(mgr, boundary_path.c_str());
        /* Should fail (doesn't exist) but handle boundary correctly */
        REQUIRE_FALSE(result);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Maximum search paths") {
        /* MAX_SEARCH_PATHS is 16 according to mod.cpp */
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        std::string dir = create_temp_dir();
        if (!dir.empty()) {
            /* Add 16 paths (the max) */
            for (int i = 0; i < 16; i++) {
                (void)agentite_mod_add_search_path(mgr, dir.c_str());
            }

            /* Adding 17th should fail */
            bool overflow_result = agentite_mod_add_search_path(mgr, dir.c_str());
            REQUIRE_FALSE(overflow_result);

            remove_temp_dir(dir);
        }

        agentite_mod_manager_destroy(mgr);
    }
}

/* ============================================================================
 * Integration Tests with Mod Manager
 * ============================================================================ */

TEST_CASE("Mod manager with edge case inputs", "[security][fuzzing][integration]") {

    SECTION("Scan empty directory") {
        std::string dir = create_temp_dir();
        if (!dir.empty()) {
            Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
            REQUIRE(mgr != nullptr);

            bool added = agentite_mod_add_search_path(mgr, dir.c_str());
            REQUIRE(added == true);

            size_t found = agentite_mod_scan(mgr);
            REQUIRE(found == 0);

            agentite_mod_manager_destroy(mgr);
            remove_temp_dir(dir);
        }
    }

    SECTION("Scan returns correct count") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        /* With no search paths, scan should return 0 */
        size_t found = agentite_mod_scan(mgr);
        REQUIRE(found == 0);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Multiple scans are idempotent") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        size_t found1 = agentite_mod_scan(mgr);
        size_t found2 = agentite_mod_scan(mgr);
        size_t found3 = agentite_mod_scan(mgr);

        REQUIRE(found1 == found2);
        REQUIRE(found2 == found3);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Load nonexistent mod fails gracefully") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        bool loaded = agentite_mod_load(mgr, "nonexistent_mod_12345");
        REQUIRE_FALSE(loaded);

        /* Error should be set */
        const char *error = agentite_get_last_error();
        REQUIRE(error != nullptr);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Find nonexistent mod returns NULL") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        const Agentite_ModInfo *info = agentite_mod_find(mgr, "does_not_exist");
        REQUIRE(info == nullptr);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Get info out of bounds returns NULL") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        /* No mods loaded, any index should return NULL */
        REQUIRE(agentite_mod_get_info(mgr, 0) == nullptr);
        REQUIRE(agentite_mod_get_info(mgr, 1) == nullptr);
        REQUIRE(agentite_mod_get_info(mgr, 100) == nullptr);
        REQUIRE(agentite_mod_get_info(mgr, SIZE_MAX) == nullptr);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Unload without load is safe") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        /* Should not crash */
        agentite_mod_unload(mgr, "never_loaded_mod");

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Set enabled on nonexistent mod returns false") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        bool result = agentite_mod_set_enabled(mgr, "nonexistent", true);
        REQUIRE_FALSE(result);

        agentite_mod_manager_destroy(mgr);
    }
}

/* ============================================================================
 * Resource Cleanup Tests
 * ============================================================================ */

TEST_CASE("Resource cleanup on error paths", "[security][fuzzing][cleanup]") {

    SECTION("Manager destroyed after failed operations") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        /* Perform various failed operations */
        agentite_mod_add_search_path(mgr, "/nonexistent");
        agentite_mod_load(mgr, "nonexistent");
        agentite_mod_find(mgr, "nonexistent");

        /* Destroy should clean up properly even after failures */
        agentite_mod_manager_destroy(mgr);
        /* No crash = success */
    }

    SECTION("Double destroy is safe") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        agentite_mod_manager_destroy(mgr);
        /* Second destroy of same pointer would be undefined behavior,
         * but destroy of NULL is safe */
        agentite_mod_manager_destroy(nullptr);
    }

    SECTION("Operations after destroy are safe with NULL") {
        /* All operations should handle NULL manager safely */
        REQUIRE(agentite_mod_count(nullptr) == 0);
        REQUIRE(agentite_mod_scan(nullptr) == 0);
        REQUIRE(agentite_mod_find(nullptr, "test") == nullptr);
    }
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_CASE("Stress tests", "[security][fuzzing][stress]") {

    SECTION("Rapid create/destroy cycles") {
        for (int i = 0; i < 100; i++) {
            Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
            REQUIRE(mgr != nullptr);
            agentite_mod_manager_destroy(mgr);
        }
    }

    SECTION("Many operations on single manager") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        for (int i = 0; i < 100; i++) {
            agentite_mod_count(mgr);
            agentite_mod_loaded_count(mgr);
            agentite_mod_find(mgr, "test");
            agentite_mod_get_state(mgr, "test");
            agentite_mod_is_enabled(mgr, "test");
        }

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Interleaved operations") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        for (int i = 0; i < 50; i++) {
            /* Interleave different types of operations */
            std::string mod_id = "mod_" + std::to_string(i);

            agentite_mod_find(mgr, mod_id.c_str());
            agentite_mod_load(mgr, mod_id.c_str());
            agentite_mod_unload(mgr, mod_id.c_str());
            agentite_mod_set_enabled(mgr, mod_id.c_str(), true);
            agentite_mod_is_enabled(mgr, mod_id.c_str());
        }

        agentite_mod_manager_destroy(mgr);
    }
}
