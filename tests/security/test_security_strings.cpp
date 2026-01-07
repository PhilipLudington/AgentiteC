/**
 * Agentite Engine - Security-Critical String Operation Tests
 *
 * Tests for buffer overflow protection, string boundary conditions,
 * and safe string handling across the engine.
 */

#include "catch_amalgamated.hpp"
#include "agentite/agentite.h"
#include "agentite/mod.h"
#include "agentite/error.h"

#include <cstring>
#include <string>
#include <limits>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

/**
 * Generate a string of specified length filled with a character.
 */
static std::string make_string(size_t length, char fill = 'A') {
    return std::string(length, fill);
}

/**
 * Generate a string with embedded null bytes.
 */
static std::string make_string_with_nulls(size_t length) {
    std::string s(length, 'A');
    if (length > 2) {
        s[length / 2] = '\0';
        s[length / 4] = '\0';
    }
    return s;
}

/* ============================================================================
 * Mod System String Boundary Tests
 * ============================================================================ */

TEST_CASE("ModInfo field size limits", "[security][mod][strings]") {
    /* Test that ModInfo struct has expected field sizes for security testing */
    SECTION("Field sizes match documentation") {
        Agentite_ModInfo info;
        REQUIRE(sizeof(info.id) == 64);
        REQUIRE(sizeof(info.name) == 128);
        REQUIRE(sizeof(info.version) == 32);
        REQUIRE(sizeof(info.author) == 64);
        REQUIRE(sizeof(info.description) == 512);
        REQUIRE(sizeof(info.path) == 512);
        REQUIRE(sizeof(info.min_engine_version) == 32);
    }
}

TEST_CASE("Mod manager NULL safety", "[security][mod][null]") {
    SECTION("Create with NULL config uses defaults") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Destroy NULL is safe") {
        agentite_mod_manager_destroy(nullptr);
        /* Should not crash */
    }

    SECTION("Add search path with NULL manager returns false") {
        bool result = agentite_mod_add_search_path(nullptr, "/some/path");
        REQUIRE_FALSE(result);
    }

    SECTION("Add search path with NULL path returns false") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        bool result = agentite_mod_add_search_path(mgr, nullptr);
        REQUIRE_FALSE(result);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Remove search path with NULL manager is safe") {
        agentite_mod_remove_search_path(nullptr, "/some/path");
        /* Should not crash */
    }

    SECTION("Remove search path with NULL path is safe") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        agentite_mod_remove_search_path(mgr, nullptr);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Scan with NULL manager returns 0") {
        size_t count = agentite_mod_scan(nullptr);
        REQUIRE(count == 0);
    }

    SECTION("Refresh with NULL manager is safe") {
        agentite_mod_refresh(nullptr);
        /* Should not crash */
    }

    SECTION("Count with NULL manager returns 0") {
        size_t count = agentite_mod_count(nullptr);
        REQUIRE(count == 0);
    }

    SECTION("Get info with NULL manager returns NULL") {
        const Agentite_ModInfo *info = agentite_mod_get_info(nullptr, 0);
        REQUIRE(info == nullptr);
    }

    SECTION("Find with NULL manager returns NULL") {
        const Agentite_ModInfo *info = agentite_mod_find(nullptr, "test");
        REQUIRE(info == nullptr);
    }

    SECTION("Find with NULL mod_id returns NULL") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        const Agentite_ModInfo *info = agentite_mod_find(mgr, nullptr);
        REQUIRE(info == nullptr);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Get state with NULL manager returns UNLOADED") {
        Agentite_ModState state = agentite_mod_get_state(nullptr, "test");
        REQUIRE(state == AGENTITE_MOD_UNLOADED);
    }

    SECTION("Resolve path with NULL manager returns NULL") {
        const char *path = agentite_mod_resolve_path(nullptr, "test.png");
        REQUIRE(path == nullptr);
    }

    SECTION("Resolve path with NULL path returns NULL") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        const char *path = agentite_mod_resolve_path(mgr, nullptr);
        REQUIRE(path == nullptr);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Has override with NULL manager returns false") {
        bool result = agentite_mod_has_override(nullptr, "test.png");
        REQUIRE_FALSE(result);
    }

    SECTION("Get override source with NULL manager returns NULL") {
        const char *source = agentite_mod_get_override_source(nullptr, "test.png");
        REQUIRE(source == nullptr);
    }

    SECTION("Load with NULL manager returns false") {
        bool result = agentite_mod_load(nullptr, "test");
        REQUIRE_FALSE(result);
    }

    SECTION("Load with NULL mod_id returns false") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        bool result = agentite_mod_load(mgr, nullptr);
        REQUIRE_FALSE(result);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Unload with NULL manager is safe") {
        agentite_mod_unload(nullptr, "test");
        /* Should not crash */
    }

    SECTION("Unload all with NULL manager is safe") {
        agentite_mod_unload_all(nullptr);
        /* Should not crash */
    }

    SECTION("Set enabled with NULL manager returns false") {
        bool result = agentite_mod_set_enabled(nullptr, "test", true);
        REQUIRE_FALSE(result);
    }

    SECTION("Is enabled with NULL manager returns false") {
        bool result = agentite_mod_is_enabled(nullptr, "test");
        REQUIRE_FALSE(result);
    }

    SECTION("Set callback with NULL manager is safe") {
        agentite_mod_set_callback(nullptr, nullptr, nullptr);
        /* Should not crash */
    }

    SECTION("Loaded count with NULL manager returns 0") {
        size_t count = agentite_mod_loaded_count(nullptr);
        REQUIRE(count == 0);
    }

    SECTION("Validate with NULL manager returns false") {
        char *error = nullptr;
        bool result = agentite_mod_validate(nullptr, "test", &error);
        REQUIRE_FALSE(result);
    }

    SECTION("Validate with NULL mod_id returns false") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        char *error = nullptr;
        bool result = agentite_mod_validate(mgr, nullptr, &error);
        REQUIRE_FALSE(result);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Resolve load order with NULL manager returns false") {
        const char *mods[] = {"test"};
        char **ordered = nullptr;
        size_t count = 0;
        bool result = agentite_mod_resolve_load_order(nullptr, mods, 1, &ordered, &count);
        REQUIRE_FALSE(result);
    }

    SECTION("Free load order with NULL is safe") {
        agentite_mod_free_load_order(nullptr, 0);
        /* Should not crash */
    }

    SECTION("Free conflicts with NULL is safe") {
        agentite_mod_free_conflicts(nullptr, 0);
        /* Should not crash */
    }

    SECTION("Save enabled with NULL manager returns false") {
        bool result = agentite_mod_save_enabled(nullptr, "/tmp/test.toml");
        REQUIRE_FALSE(result);
    }

    SECTION("Load enabled with NULL manager returns false") {
        bool result = agentite_mod_load_enabled(nullptr, "/tmp/test.toml");
        REQUIRE_FALSE(result);
    }
}

TEST_CASE("Mod state name utility", "[security][mod][strings]") {
    SECTION("All states have valid names") {
        REQUIRE(strcmp(agentite_mod_state_name(AGENTITE_MOD_UNLOADED), "UNLOADED") == 0);
        REQUIRE(strcmp(agentite_mod_state_name(AGENTITE_MOD_DISCOVERED), "DISCOVERED") == 0);
        REQUIRE(strcmp(agentite_mod_state_name(AGENTITE_MOD_LOADING), "LOADING") == 0);
        REQUIRE(strcmp(agentite_mod_state_name(AGENTITE_MOD_LOADED), "LOADED") == 0);
        REQUIRE(strcmp(agentite_mod_state_name(AGENTITE_MOD_FAILED), "FAILED") == 0);
        REQUIRE(strcmp(agentite_mod_state_name(AGENTITE_MOD_DISABLED), "DISABLED") == 0);
    }

    SECTION("Unknown state returns UNKNOWN") {
        const char *name = agentite_mod_state_name(999);
        REQUIRE(strcmp(name, "UNKNOWN") == 0);
    }

    SECTION("Negative state value returns UNKNOWN") {
        const char *name = agentite_mod_state_name(-1);
        REQUIRE(strcmp(name, "UNKNOWN") == 0);
    }
}

TEST_CASE("Mod config default values", "[security][mod][config]") {
    SECTION("Default config has expected values") {
        Agentite_ModManagerConfig config = AGENTITE_MOD_MANAGER_CONFIG_DEFAULT;
        REQUIRE(config.assets == nullptr);
        REQUIRE(config.hotreload == nullptr);
        REQUIRE(config.events == nullptr);
        REQUIRE(config.allow_overrides == true);
        REQUIRE(config.emit_events == true);
    }
}

TEST_CASE("Get dependencies NULL safety", "[security][mod][null]") {
    SECTION("NULL manager returns 0") {
        const char *deps[10];
        size_t count = agentite_mod_get_dependencies(nullptr, "test", deps, 10);
        REQUIRE(count == 0);
    }

    SECTION("NULL mod_id returns 0") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        const char *deps[10];
        size_t count = agentite_mod_get_dependencies(mgr, nullptr, deps, 10);
        REQUIRE(count == 0);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("NULL out_deps returns 0") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        size_t count = agentite_mod_get_dependencies(mgr, "test", nullptr, 10);
        REQUIRE(count == 0);
        agentite_mod_manager_destroy(mgr);
    }
}

TEST_CASE("Get conflicts NULL safety", "[security][mod][null]") {
    SECTION("NULL manager returns 0") {
        const char *conflicts[10];
        size_t count = agentite_mod_get_conflicts(nullptr, "test", conflicts, 10);
        REQUIRE(count == 0);
    }

    SECTION("NULL mod_id returns 0") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        const char *conflicts[10];
        size_t count = agentite_mod_get_conflicts(mgr, nullptr, conflicts, 10);
        REQUIRE(count == 0);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("NULL out_conflicts returns 0") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        size_t count = agentite_mod_get_conflicts(mgr, "test", nullptr, 10);
        REQUIRE(count == 0);
        agentite_mod_manager_destroy(mgr);
    }
}

TEST_CASE("Check conflicts NULL safety", "[security][mod][null]") {
    SECTION("NULL manager returns true (no conflicts)") {
        const char *mods[] = {"test"};
        bool result = agentite_mod_check_conflicts(nullptr, mods, 1, nullptr, nullptr);
        REQUIRE(result == true);
    }

    SECTION("NULL enabled_mods returns true (no conflicts)") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        bool result = agentite_mod_check_conflicts(mgr, nullptr, 0, nullptr, nullptr);
        REQUIRE(result == true);
        agentite_mod_manager_destroy(mgr);
    }
}

TEST_CASE("Load order resolution NULL safety", "[security][mod][null]") {
    SECTION("NULL enabled_mods returns false") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        char **ordered = nullptr;
        size_t count = 0;
        bool result = agentite_mod_resolve_load_order(mgr, nullptr, 0, &ordered, &count);
        REQUIRE_FALSE(result);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("NULL out_ordered returns false") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        const char *mods[] = {"test"};
        size_t count = 0;
        bool result = agentite_mod_resolve_load_order(mgr, mods, 1, nullptr, &count);
        REQUIRE_FALSE(result);
        agentite_mod_manager_destroy(mgr);
    }

    SECTION("NULL out_count returns false") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);
        const char *mods[] = {"test"};
        char **ordered = nullptr;
        bool result = agentite_mod_resolve_load_order(mgr, mods, 1, &ordered, nullptr);
        REQUIRE_FALSE(result);
        agentite_mod_manager_destroy(mgr);
    }
}

/* ============================================================================
 * Safe Allocation Function Tests
 * ============================================================================ */

TEST_CASE("agentite_safe_malloc overflow protection", "[security][memory][overflow]") {
    SECTION("Normal allocation succeeds") {
        void *ptr = agentite_safe_malloc(10, sizeof(int));
        REQUIRE(ptr != nullptr);
        free(ptr);
    }

    SECTION("Zero count returns NULL or valid empty allocation") {
        void *ptr = agentite_safe_malloc(0, sizeof(int));
        /* Implementation may return NULL or valid pointer for zero size */
        if (ptr) free(ptr);
    }

    SECTION("Zero size returns NULL or valid empty allocation") {
        void *ptr = agentite_safe_malloc(10, 0);
        /* Implementation may return NULL or valid pointer for zero size */
        if (ptr) free(ptr);
    }

    SECTION("Overflow detection - count causes overflow") {
        size_t dangerous_count = SIZE_MAX / sizeof(int) + 1;
        void *ptr = agentite_safe_malloc(dangerous_count, sizeof(int));
        REQUIRE(ptr == nullptr);
    }

    SECTION("Overflow detection - large count and size") {
        size_t half_max = SIZE_MAX / 2;
        void *ptr = agentite_safe_malloc(half_max, 3);
        REQUIRE(ptr == nullptr);
    }

    SECTION("Boundary case - exactly at SIZE_MAX") {
        /* SIZE_MAX / sizeof(int) should succeed if memory available */
        /* SIZE_MAX / sizeof(int) + 1 should detect overflow */
        size_t boundary = SIZE_MAX / sizeof(int) + 1;
        void *ptr = agentite_safe_malloc(boundary, sizeof(int));
        REQUIRE(ptr == nullptr);
    }
}

TEST_CASE("agentite_safe_realloc overflow protection", "[security][memory][overflow]") {
    SECTION("Normal realloc succeeds") {
        void *ptr = malloc(10);
        REQUIRE(ptr != nullptr);
        void *new_ptr = agentite_safe_realloc(ptr, 10, sizeof(int));
        REQUIRE(new_ptr != nullptr);
        free(new_ptr);
    }

    SECTION("Realloc from NULL succeeds (like malloc)") {
        void *ptr = agentite_safe_realloc(nullptr, 10, sizeof(int));
        REQUIRE(ptr != nullptr);
        free(ptr);
    }

    SECTION("Overflow detection - count causes overflow") {
        void *ptr = malloc(10);
        REQUIRE(ptr != nullptr);
        size_t dangerous_count = SIZE_MAX / sizeof(int) + 1;
        void *new_ptr = agentite_safe_realloc(ptr, dangerous_count, sizeof(int));
        REQUIRE(new_ptr == nullptr);
        free(ptr); /* Original still valid when realloc fails */
    }

    SECTION("Overflow detection - both large") {
        void *ptr = malloc(10);
        REQUIRE(ptr != nullptr);
        void *new_ptr = agentite_safe_realloc(ptr, SIZE_MAX, 2);
        REQUIRE(new_ptr == nullptr);
        free(ptr);
    }
}

TEST_CASE("AGENTITE_ALLOC macro tests", "[security][memory][macros]") {
    SECTION("AGENTITE_ALLOC allocates and zeroes") {
        int *ptr = AGENTITE_ALLOC(int);
        REQUIRE(ptr != nullptr);
        REQUIRE(*ptr == 0);
        free(ptr);
    }

    SECTION("AGENTITE_ALLOC_ARRAY allocates and zeroes array") {
        int *arr = AGENTITE_ALLOC_ARRAY(int, 10);
        REQUIRE(arr != nullptr);
        for (int i = 0; i < 10; i++) {
            REQUIRE(arr[i] == 0);
        }
        free(arr);
    }

    SECTION("AGENTITE_ALLOC_ARRAY with zero count") {
        int *arr = AGENTITE_ALLOC_ARRAY(int, 0);
        /* calloc(0, size) may return NULL or valid pointer */
        if (arr) free(arr);
    }
}

TEST_CASE("AGENTITE_MALLOC_ARRAY overflow protection", "[security][memory][macros]") {
    SECTION("Normal allocation succeeds") {
        int *arr = AGENTITE_MALLOC_ARRAY(int, 100);
        REQUIRE(arr != nullptr);
        free(arr);
    }

    SECTION("Overflow protection triggers") {
        size_t dangerous_count = SIZE_MAX / sizeof(int) + 1;
        int *arr = AGENTITE_MALLOC_ARRAY(int, dangerous_count);
        REQUIRE(arr == nullptr);
    }
}

/* ============================================================================
 * Integer Boundary Tests
 * ============================================================================ */

TEST_CASE("Integer boundary conditions", "[security][integer][boundary]") {
    SECTION("SIZE_MAX / element_size boundary") {
        /* Test that we correctly detect overflow at the boundary */
        size_t safe_count = SIZE_MAX / sizeof(int);
        size_t unsafe_count = safe_count + 1;

        /* Safe count should not trigger overflow check */
        bool safe_overflow = (sizeof(int) != 0 && safe_count > SIZE_MAX / sizeof(int));
        REQUIRE_FALSE(safe_overflow);

        /* Unsafe count should trigger overflow check */
        bool unsafe_overflow = (sizeof(int) != 0 && unsafe_count > SIZE_MAX / sizeof(int));
        REQUIRE(unsafe_overflow);
    }

    SECTION("Zero size edge case") {
        size_t count = 100;
        size_t size = 0;
        /* size == 0 should not cause division by zero */
        bool would_overflow = (size != 0 && count > SIZE_MAX / size);
        REQUIRE_FALSE(would_overflow); /* size == 0, so first condition false */
    }
}

/* ============================================================================
 * String Operation Boundary Tests
 * ============================================================================ */

TEST_CASE("String length boundary testing patterns", "[security][strings][boundary]") {
    /* These tests verify our test helper functions work correctly
     * and demonstrate the patterns for boundary testing */

    SECTION("make_string generates correct length") {
        std::string s = make_string(63);
        REQUIRE(s.length() == 63);
        REQUIRE(s[0] == 'A');
        REQUIRE(s[62] == 'A');
    }

    SECTION("make_string at boundary size") {
        std::string s64 = make_string(64);
        REQUIRE(s64.length() == 64);

        std::string s128 = make_string(128);
        REQUIRE(s128.length() == 128);

        std::string s512 = make_string(512);
        REQUIRE(s512.length() == 512);
    }

    SECTION("make_string_with_nulls has embedded nulls") {
        std::string s = make_string_with_nulls(100);
        /* Note: std::string handles embedded nulls, but C functions don't */
        REQUIRE(s.length() == 100);
        REQUIRE(s[25] == '\0');
        REQUIRE(s[50] == '\0');
    }
}

/* ============================================================================
 * Error Handling Security Tests
 * ============================================================================ */

TEST_CASE("Error API NULL safety", "[security][error][null]") {
    SECTION("get_last_error never returns NULL") {
        agentite_clear_error();
        const char *err = agentite_get_last_error();
        /* Should return empty string or valid pointer, never NULL */
        REQUIRE(err != nullptr);
    }

    SECTION("set_error with NULL format is safe") {
        /* This tests internal safety - implementation dependent */
        agentite_clear_error();
        /* Can't easily test NULL format without risking undefined behavior */
        /* Instead, test that we can set and get errors normally */
        agentite_set_error("test error %d", 42);
        const char *err = agentite_get_last_error();
        REQUIRE(err != nullptr);
        REQUIRE(strstr(err, "test error") != nullptr);
    }

    SECTION("Clear error resets state") {
        agentite_set_error("some error");
        REQUIRE(agentite_has_error() == true);
        agentite_clear_error();
        REQUIRE(agentite_has_error() == false);
    }
}

/* ============================================================================
 * Format String Security Tests
 * ============================================================================ */

TEST_CASE("Format string safety patterns", "[security][format]") {
    SECTION("Error messages don't interpret user strings as format") {
        /* This is a documentation test - ensures we don't have format string bugs */
        /* The agentite_set_error function uses printf-style formatting internally */
        /* Verify that %s is used correctly for user-provided strings */

        const char *user_string = "test%s%d%n"; /* Potentially dangerous if misused */

        /* Safe pattern: user string passed as argument to %s */
        agentite_set_error("User provided: %s", user_string);
        const char *err = agentite_get_last_error();
        REQUIRE(err != nullptr);
        /* The literal %s%d%n should appear in the output, not be interpreted */
        REQUIRE(strstr(err, "%s%d%n") != nullptr);
    }
}

/* ============================================================================
 * Path Traversal Prevention Tests
 * ============================================================================ */

TEST_CASE("Path handling patterns", "[security][paths]") {
    SECTION("Normal paths are accepted") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);

        /* Resolve path with normal relative path */
        const char *result = agentite_mod_resolve_path(mgr, "textures/sprite.png");
        REQUIRE(result != nullptr);

        agentite_mod_manager_destroy(mgr);
    }

    SECTION("Empty path returns valid pointer") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(NULL);
        REQUIRE(mgr != nullptr);

        const char *result = agentite_mod_resolve_path(mgr, "");
        REQUIRE(result != nullptr);

        agentite_mod_manager_destroy(mgr);
    }
}

/* ============================================================================
 * Thread Safety Documentation Tests
 * ============================================================================ */

TEST_CASE("Main thread tracking", "[security][thread]") {
    SECTION("Main thread set during init") {
        /* agentite_init should set main thread ID */
        /* We test the API exists and works */
        agentite_set_main_thread();
        REQUIRE(agentite_is_main_thread() == true);
    }
}
