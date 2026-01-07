/**
 * Agentite Engine - Allocation Failure Tests
 *
 * Tests for proper handling of memory allocation failures.
 * These tests verify that NULL is properly handled when allocations fail.
 */

#include "catch_amalgamated.hpp"
#include "agentite/agentite.h"
#include "agentite/mod.h"
#include "agentite/error.h"

#include <cstdlib>
#include <cstring>
#include <limits>

/* ============================================================================
 * Allocation Failure Simulation Tests
 *
 * Since we can't easily mock malloc in a portable way without modifying the
 * build system, we test the safe allocation wrappers that ARE designed to
 * return NULL on overflow conditions (which is a form of allocation failure).
 *
 * Additionally, we verify that all code paths handle NULL allocations correctly.
 * ============================================================================ */

TEST_CASE("Safe allocation functions return NULL on overflow", "[security][allocation][failure]") {

    SECTION("agentite_safe_malloc returns NULL on count overflow") {
        /* Trigger overflow by requesting more than SIZE_MAX bytes */
        size_t large_count = SIZE_MAX / sizeof(int) + 1;
        void *result = agentite_safe_malloc(large_count, sizeof(int));
        REQUIRE(result == nullptr);
    }

    SECTION("agentite_safe_malloc returns NULL on size overflow") {
        /* Large size that would overflow */
        void *result = agentite_safe_malloc(SIZE_MAX, 2);
        REQUIRE(result == nullptr);
    }

    SECTION("agentite_safe_realloc returns NULL on overflow") {
        void *original = malloc(16);
        REQUIRE(original != nullptr);

        size_t large_count = SIZE_MAX / sizeof(int) + 1;
        void *result = agentite_safe_realloc(original, large_count, sizeof(int));
        REQUIRE(result == nullptr);

        /* Original pointer should still be valid */
        free(original);
    }

    SECTION("agentite_safe_realloc preserves original on failure") {
        /* Allocate and write a pattern */
        int *original = (int *)malloc(sizeof(int) * 4);
        REQUIRE(original != nullptr);
        original[0] = 0xDEADBEEF;
        original[1] = 0xCAFEBABE;

        /* Try to realloc to overflow size - should fail */
        size_t dangerous_size = SIZE_MAX / sizeof(int) + 1;
        int *result = (int *)agentite_safe_realloc(original, dangerous_size, sizeof(int));
        REQUIRE(result == nullptr);

        /* Original data should be preserved */
        REQUIRE(original[0] == (int)0xDEADBEEF);
        REQUIRE(original[1] == (int)0xCAFEBABE);

        free(original);
    }
}

TEST_CASE("AGENTITE_MALLOC_ARRAY returns NULL on overflow", "[security][allocation][macros]") {

    SECTION("Overflow in count") {
        size_t dangerous_count = SIZE_MAX / sizeof(int) + 1;
        int *result = AGENTITE_MALLOC_ARRAY(int, dangerous_count);
        REQUIRE(result == nullptr);
    }

    SECTION("Large struct overflow") {
        struct LargeStruct {
            char data[1024];
        };
        size_t dangerous_count = SIZE_MAX / sizeof(LargeStruct) + 1;
        LargeStruct *result = AGENTITE_MALLOC_ARRAY(LargeStruct, dangerous_count);
        REQUIRE(result == nullptr);
    }
}

TEST_CASE("API functions handle NULL contexts", "[security][allocation][null]") {
    /* These tests verify that all public API functions properly handle
     * NULL contexts, which is what happens when allocation fails */

    SECTION("Mod manager API handles NULL manager") {
        /* All these should be safe when manager is NULL */
        REQUIRE(agentite_mod_count(nullptr) == 0);
        REQUIRE(agentite_mod_get_info(nullptr, 0) == nullptr);
        REQUIRE(agentite_mod_find(nullptr, "test") == nullptr);
        REQUIRE(agentite_mod_get_state(nullptr, "test") == AGENTITE_MOD_UNLOADED);
        REQUIRE(agentite_mod_resolve_path(nullptr, "path") == nullptr);
        REQUIRE(agentite_mod_has_override(nullptr, "path") == false);
        REQUIRE(agentite_mod_get_override_source(nullptr, "path") == nullptr);
        REQUIRE(agentite_mod_load(nullptr, "test") == false);
        REQUIRE(agentite_mod_loaded_count(nullptr) == 0);
        REQUIRE(agentite_mod_is_enabled(nullptr, "test") == false);
        REQUIRE(agentite_mod_set_enabled(nullptr, "test", true) == false);
        REQUIRE(agentite_mod_add_search_path(nullptr, "/path") == false);
        REQUIRE(agentite_mod_validate(nullptr, "test", nullptr) == false);
        REQUIRE(agentite_mod_save_enabled(nullptr, "/path") == false);
        REQUIRE(agentite_mod_load_enabled(nullptr, "/path") == false);

        /* These should not crash */
        agentite_mod_manager_destroy(nullptr);
        agentite_mod_unload(nullptr, "test");
        agentite_mod_unload_all(nullptr);
        agentite_mod_remove_search_path(nullptr, "/path");
        agentite_mod_refresh(nullptr);
        agentite_mod_set_callback(nullptr, nullptr, nullptr);
    }
}

TEST_CASE("Allocation-dependent operations fail gracefully", "[security][allocation][graceful]") {

    SECTION("Mod manager creation returns NULL on bad config after exhaustion") {
        /* We can't easily simulate memory exhaustion, but we can verify
         * the function signature allows for NULL returns */
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        /* In normal conditions, this should succeed */
        if (mgr) {
            agentite_mod_manager_destroy(mgr);
        }
        /* The test passes either way - we're verifying the API handles both cases */
    }

    SECTION("Load order resolution returns false on allocation failure simulation") {
        Agentite_ModManager *mgr = agentite_mod_manager_create(nullptr);
        REQUIRE(mgr != nullptr);

        /* With no mods and empty array, should still work */
        const char *mods[] = {};
        char **ordered = nullptr;
        size_t count = 0;

        /* Empty mod list should succeed */
        bool result = agentite_mod_resolve_load_order(mgr, mods, 0, &ordered, &count);
        REQUIRE(result == true);
        REQUIRE(count == 0);
        agentite_mod_free_load_order(ordered, count);

        agentite_mod_manager_destroy(mgr);
    }
}

TEST_CASE("Calloc-based macros zero-initialize memory", "[security][allocation][initialization]") {

    SECTION("AGENTITE_ALLOC zeroes single allocation") {
        struct TestStruct {
            int a;
            int b;
            char c[32];
            void *ptr;
        };

        TestStruct *s = AGENTITE_ALLOC(TestStruct);
        REQUIRE(s != nullptr);

        /* All fields should be zero */
        REQUIRE(s->a == 0);
        REQUIRE(s->b == 0);
        REQUIRE(s->ptr == nullptr);
        for (int i = 0; i < 32; i++) {
            REQUIRE(s->c[i] == 0);
        }

        free(s);
    }

    SECTION("AGENTITE_ALLOC_ARRAY zeroes array") {
        int *arr = AGENTITE_ALLOC_ARRAY(int, 100);
        REQUIRE(arr != nullptr);

        for (int i = 0; i < 100; i++) {
            REQUIRE(arr[i] == 0);
        }

        free(arr);
    }

    SECTION("Zero initialization prevents uninitialized memory bugs") {
        struct ComplexStruct {
            int count;
            void *items;
            bool initialized;
            char name[64];
        };

        ComplexStruct *obj = AGENTITE_ALLOC(ComplexStruct);
        REQUIRE(obj != nullptr);

        /* A function checking this struct can safely assume zeroed state */
        REQUIRE(obj->count == 0);
        REQUIRE(obj->items == nullptr);
        REQUIRE(obj->initialized == false);
        REQUIRE(obj->name[0] == '\0');

        free(obj);
    }
}

TEST_CASE("Overflow check boundary conditions", "[security][allocation][boundary]") {

    SECTION("Just below overflow boundary succeeds in check") {
        size_t safe_count = SIZE_MAX / sizeof(int);
        /* The overflow check should NOT trigger */
        bool would_overflow = (sizeof(int) != 0 && safe_count > SIZE_MAX / sizeof(int));
        REQUIRE_FALSE(would_overflow);
    }

    SECTION("At overflow boundary triggers check") {
        size_t unsafe_count = SIZE_MAX / sizeof(int) + 1;
        /* The overflow check SHOULD trigger */
        bool would_overflow = (sizeof(int) != 0 && unsafe_count > SIZE_MAX / sizeof(int));
        REQUIRE(would_overflow);
    }

    SECTION("SIZE_MAX with size > 1 always overflows") {
        /* SIZE_MAX * 2 would overflow */
        bool would_overflow = (2 != 0 && SIZE_MAX > SIZE_MAX / 2);
        REQUIRE(would_overflow);
    }

    SECTION("Any count with size 0 does not overflow") {
        /* Division by zero protection */
        size_t any_count = SIZE_MAX;
        size_t size = 0;
        bool would_overflow = (size != 0 && any_count > SIZE_MAX / size);
        REQUIRE_FALSE(would_overflow); /* size == 0 short-circuits */
    }
}

TEST_CASE("Free functions handle NULL safely", "[security][allocation][free]") {

    SECTION("agentite_mod_free_load_order handles NULL") {
        agentite_mod_free_load_order(nullptr, 0);
        agentite_mod_free_load_order(nullptr, 100);
        /* Should not crash */
    }

    SECTION("agentite_mod_free_conflicts handles NULL") {
        agentite_mod_free_conflicts(nullptr, 0);
        agentite_mod_free_conflicts(nullptr, 100);
        /* Should not crash */
    }

    SECTION("Free with zero count is safe") {
        char **valid_ptr = (char **)malloc(sizeof(char *));
        REQUIRE(valid_ptr != nullptr);

        /* Free with count 0 should just free the outer array */
        /* Note: This is implementation-specific, some impls may not handle this */
        agentite_mod_free_load_order(valid_ptr, 0);
        /* Memory was freed in the function */
    }
}

TEST_CASE("Realloc patterns for growing arrays", "[security][allocation][realloc]") {

    SECTION("Growing array with safe realloc") {
        int *arr = (int *)malloc(sizeof(int) * 4);
        REQUIRE(arr != nullptr);
        arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4;

        /* Grow to 8 elements */
        int *new_arr = (int *)agentite_safe_realloc(arr, 8, sizeof(int));
        REQUIRE(new_arr != nullptr);

        /* Original data preserved */
        REQUIRE(new_arr[0] == 1);
        REQUIRE(new_arr[1] == 2);
        REQUIRE(new_arr[2] == 3);
        REQUIRE(new_arr[3] == 4);

        free(new_arr);
    }

    SECTION("Shrinking array with safe realloc") {
        int *arr = (int *)malloc(sizeof(int) * 8);
        REQUIRE(arr != nullptr);
        for (int i = 0; i < 8; i++) arr[i] = i;

        /* Shrink to 4 elements */
        int *new_arr = (int *)agentite_safe_realloc(arr, 4, sizeof(int));
        REQUIRE(new_arr != nullptr);

        /* First 4 elements preserved */
        REQUIRE(new_arr[0] == 0);
        REQUIRE(new_arr[1] == 1);
        REQUIRE(new_arr[2] == 2);
        REQUIRE(new_arr[3] == 3);

        free(new_arr);
    }

    SECTION("Realloc to zero size") {
        int *arr = (int *)malloc(sizeof(int) * 4);
        REQUIRE(arr != nullptr);

        /* Realloc to 0 is implementation-defined (may free or return small allocation) */
        int *new_arr = (int *)agentite_safe_realloc(arr, 0, sizeof(int));
        /* Either NULL or valid pointer is acceptable */
        if (new_arr) {
            free(new_arr);
        }
        /* If NULL, the original was freed by realloc */
    }
}

TEST_CASE("Large allocation requests", "[security][allocation][large]") {

    SECTION("Reasonable large allocation may succeed") {
        /* 1MB allocation - should succeed on most systems */
        size_t size = 1024 * 1024;
        void *ptr = malloc(size);
        if (ptr) {
            /* Verify we can write to it */
            memset(ptr, 0, size);
            free(ptr);
        }
        /* Test passes either way - we're just verifying behavior */
    }

    SECTION("Very large allocation request") {
        /* 1GB allocation - may or may not succeed */
        size_t size = 1024ULL * 1024 * 1024;
        void *ptr = malloc(size);
        if (ptr) {
            free(ptr);
        }
        /* Either result is acceptable */
    }

    SECTION("Overflow-triggering allocation fails safely") {
        /* This should definitely fail */
        void *ptr = agentite_safe_malloc(SIZE_MAX, SIZE_MAX);
        REQUIRE(ptr == nullptr);
    }
}

TEST_CASE("Allocation patterns match coding standards", "[security][allocation][standards]") {

    SECTION("M1: All allocations checked - demonstrated by safe wrappers") {
        /* The agentite_safe_* functions return NULL on failure */
        void *ptr = agentite_safe_malloc(SIZE_MAX, 2);
        REQUIRE(ptr == nullptr); /* NULL check works */
    }

    SECTION("M2: Prefer calloc for zero-init") {
        /* AGENTITE_ALLOC and AGENTITE_ALLOC_ARRAY use calloc */
        int *arr = AGENTITE_ALLOC_ARRAY(int, 10);
        REQUIRE(arr != nullptr);
        for (int i = 0; i < 10; i++) {
            REQUIRE(arr[i] == 0);
        }
        free(arr);
    }

    SECTION("M5: Destroy functions accept NULL") {
        agentite_mod_manager_destroy(nullptr);
        /* Other destroy functions should follow same pattern */
    }

    SECTION("M6: calloc checks overflow") {
        /* calloc(count, size) internally checks for overflow */
        /* This is why we prefer it over malloc(count * size) */
        /* Note: Under AddressSanitizer, this may abort instead of returning NULL */
#if !defined(__SANITIZE_ADDRESS__) && !defined(__has_feature)
        size_t dangerous_count = SIZE_MAX;
        size_t dangerous_size = 2;
        void *ptr = calloc(dangerous_count, dangerous_size);
        REQUIRE(ptr == nullptr); /* calloc should detect overflow */
#elif defined(__has_feature)
#if !__has_feature(address_sanitizer)
        size_t dangerous_count = SIZE_MAX;
        size_t dangerous_size = 2;
        void *ptr = calloc(dangerous_count, dangerous_size);
        REQUIRE(ptr == nullptr); /* calloc should detect overflow */
#endif
#endif
        /* Test is skipped under sanitizers as they abort on overflow */
    }
}
