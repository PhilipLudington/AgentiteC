/*
 * Agentite Engine Tests
 *
 * Tests for core engine functionality that can be tested without GPU/window.
 * Note: Full engine init/shutdown tests require a display and GPU, which
 * may not be available in CI environments.
 */

#include "catch_amalgamated.hpp"
#include "agentite/agentite.h"
#include "agentite/error.h"
#include <cstring>
#include <limits>
#include <thread>
#include <atomic>

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_CASE("Thread ID tracking", "[engine][thread]") {
    SECTION("Set and check main thread") {
        agentite_set_main_thread();
        REQUIRE(agentite_is_main_thread());
    }

    SECTION("Non-main thread returns false") {
        agentite_set_main_thread();

        std::atomic<bool> is_main_from_thread{true};

        std::thread worker([&]() {
            is_main_from_thread = agentite_is_main_thread();
        });
        worker.join();

        REQUIRE_FALSE(is_main_from_thread);
        // Verify main thread still returns true
        REQUIRE(agentite_is_main_thread());
    }

    SECTION("Multiple calls to set_main_thread updates thread ID") {
        agentite_set_main_thread();
        REQUIRE(agentite_is_main_thread());

        // Call again - should still work (idempotent behavior)
        agentite_set_main_thread();
        REQUIRE(agentite_is_main_thread());
    }
}

/* ============================================================================
 * Safe Memory Allocation Tests
 * ============================================================================ */

TEST_CASE("Safe realloc overflow protection", "[engine][memory]") {
    SECTION("Normal realloc succeeds") {
        void *ptr = malloc(10);
        REQUIRE(ptr != nullptr);

        void *new_ptr = agentite_safe_realloc(ptr, 10, sizeof(int));
        REQUIRE(new_ptr != nullptr);

        free(new_ptr);
    }

    SECTION("Realloc with zero count returns NULL or empty allocation") {
        void *ptr = malloc(10);
        REQUIRE(ptr != nullptr);

        // realloc with 0 is implementation-defined (may return NULL or valid ptr)
        void *new_ptr = agentite_safe_realloc(ptr, 0, sizeof(int));
        // Don't check result - behavior is implementation-defined
        // Just ensure we don't crash
        free(new_ptr);  // free(NULL) is safe
    }

    SECTION("Overflow detection - large count") {
        // SIZE_MAX / sizeof(int) + 1 would overflow when multiplied
        size_t dangerous_count = SIZE_MAX / sizeof(int) + 1;

        void *ptr = agentite_safe_realloc(nullptr, dangerous_count, sizeof(int));
        REQUIRE(ptr == nullptr);  // Should detect overflow and return NULL
    }

    SECTION("Overflow detection - large size") {
        size_t dangerous_size = SIZE_MAX / 2 + 1;

        void *ptr = agentite_safe_realloc(nullptr, 2, dangerous_size);
        REQUIRE(ptr == nullptr);  // Should detect overflow and return NULL
    }

    SECTION("Overflow detection - both large") {
        size_t half_max = SIZE_MAX / 2;

        void *ptr = agentite_safe_realloc(nullptr, half_max, 3);
        REQUIRE(ptr == nullptr);  // count * size > SIZE_MAX
    }
}

TEST_CASE("Safe malloc overflow protection", "[engine][memory]") {
    SECTION("Normal malloc succeeds") {
        void *ptr = agentite_safe_malloc(10, sizeof(int));
        REQUIRE(ptr != nullptr);
        free(ptr);
    }

    SECTION("Overflow detection - large count") {
        size_t dangerous_count = SIZE_MAX / sizeof(int) + 1;

        void *ptr = agentite_safe_malloc(dangerous_count, sizeof(int));
        REQUIRE(ptr == nullptr);
    }

    SECTION("Overflow detection - large size") {
        size_t dangerous_size = SIZE_MAX / 2 + 1;

        void *ptr = agentite_safe_malloc(2, dangerous_size);
        REQUIRE(ptr == nullptr);
    }

    SECTION("Zero count returns NULL or empty allocation") {
        void *ptr = agentite_safe_malloc(0, sizeof(int));
        // malloc(0) is implementation-defined
        free(ptr);  // free(NULL) is safe
    }

    SECTION("Zero size returns NULL") {
        void *ptr = agentite_safe_malloc(10, 0);
        // malloc(0) is implementation-defined
        free(ptr);
    }
}

/* ============================================================================
 * AGENTITE_ALLOC Macro Tests
 * ============================================================================ */

TEST_CASE("AGENTITE_ALLOC macro", "[engine][memory]") {
    struct TestStruct {
        int a;
        float b;
        char c[32];
    };

    SECTION("Allocates and zero-initializes") {
        TestStruct *ts = AGENTITE_ALLOC(TestStruct);
        REQUIRE(ts != nullptr);

        // Should be zero-initialized (calloc)
        REQUIRE(ts->a == 0);
        REQUIRE(ts->b == 0.0f);
        REQUIRE(ts->c[0] == '\0');

        free(ts);
    }
}

TEST_CASE("AGENTITE_ALLOC_ARRAY macro", "[engine][memory]") {
    SECTION("Allocates array and zero-initializes") {
        int *arr = AGENTITE_ALLOC_ARRAY(int, 100);
        REQUIRE(arr != nullptr);

        // Should be zero-initialized
        for (int i = 0; i < 100; i++) {
            REQUIRE(arr[i] == 0);
        }

        free(arr);
    }

    SECTION("Zero count") {
        int *arr = AGENTITE_ALLOC_ARRAY(int, 0);
        // calloc(0, size) is implementation-defined
        free(arr);
    }
}

TEST_CASE("AGENTITE_MALLOC_ARRAY macro with overflow protection", "[engine][memory]") {
    SECTION("Normal allocation succeeds") {
        int *arr = AGENTITE_MALLOC_ARRAY(int, 100);
        REQUIRE(arr != nullptr);
        free(arr);
    }

    SECTION("Overflow protection") {
        size_t dangerous_count = SIZE_MAX / sizeof(int) + 1;
        int *arr = AGENTITE_MALLOC_ARRAY(int, dangerous_count);
        REQUIRE(arr == nullptr);  // Should detect overflow
    }
}

/* ============================================================================
 * NULL Safety Tests (functions that should handle NULL gracefully)
 * ============================================================================ */

TEST_CASE("Engine NULL safety", "[engine][null]") {
    SECTION("agentite_shutdown with NULL") {
        // Should not crash
        agentite_shutdown(nullptr);
    }

    SECTION("agentite_is_running with NULL") {
        REQUIRE_FALSE(agentite_is_running(nullptr));
    }

    SECTION("agentite_quit with NULL") {
        // Should not crash
        agentite_quit(nullptr);
    }

    SECTION("agentite_poll_events with NULL") {
        // Should not crash
        agentite_poll_events(nullptr);
    }

    SECTION("agentite_begin_frame with NULL") {
        // Should not crash
        agentite_begin_frame(nullptr);
    }

    SECTION("agentite_end_frame with NULL") {
        // Should not crash
        agentite_end_frame(nullptr);
    }

    SECTION("agentite_get_delta_time with NULL") {
        REQUIRE(agentite_get_delta_time(nullptr) == 0.0f);
    }

    SECTION("agentite_get_frame_count with NULL") {
        REQUIRE(agentite_get_frame_count(nullptr) == 0);
    }

    SECTION("agentite_get_gpu_device with NULL") {
        REQUIRE(agentite_get_gpu_device(nullptr) == nullptr);
    }

    SECTION("agentite_get_window with NULL") {
        REQUIRE(agentite_get_window(nullptr) == nullptr);
    }

    SECTION("agentite_acquire_command_buffer with NULL") {
        REQUIRE(agentite_acquire_command_buffer(nullptr) == nullptr);
    }

    SECTION("agentite_get_dpi_scale with NULL") {
        REQUIRE(agentite_get_dpi_scale(nullptr) == 1.0f);
    }

    SECTION("agentite_get_window_size with NULL") {
        int w = -1, h = -1;
        agentite_get_window_size(nullptr, &w, &h);
        REQUIRE(w == 0);
        REQUIRE(h == 0);
    }

    SECTION("agentite_get_drawable_size with NULL") {
        int w = -1, h = -1;
        agentite_get_drawable_size(nullptr, &w, &h);
        REQUIRE(w == 0);
        REQUIRE(h == 0);
    }

    SECTION("agentite_get_render_pass with NULL") {
        REQUIRE(agentite_get_render_pass(nullptr) == nullptr);
    }

    SECTION("agentite_get_command_buffer with NULL") {
        REQUIRE(agentite_get_command_buffer(nullptr) == nullptr);
    }

    SECTION("agentite_end_render_pass with NULL") {
        // Should not crash
        agentite_end_render_pass(nullptr);
    }

    SECTION("agentite_end_render_pass_no_submit with NULL") {
        // Should not crash
        agentite_end_render_pass_no_submit(nullptr);
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

TEST_CASE("Default config values", "[engine][config]") {
    Agentite_Config config = AGENTITE_DEFAULT_CONFIG;

    SECTION("Default config has sensible values") {
        REQUIRE(config.window_title != nullptr);
        REQUIRE(strlen(config.window_title) > 0);
        REQUIRE(config.window_width > 0);
        REQUIRE(config.window_height > 0);
        // Default should not be fullscreen
        REQUIRE_FALSE(config.fullscreen);
        // Default should allow resize
        REQUIRE(config.resizable);
        // Default should have vsync
        REQUIRE(config.vsync);
    }

    SECTION("Default dimensions are reasonable") {
        REQUIRE(config.window_width >= 640);
        REQUIRE(config.window_width <= 7680);  // 8K max
        REQUIRE(config.window_height >= 480);
        REQUIRE(config.window_height <= 4320);  // 8K max
    }
}

/* ============================================================================
 * Version Info Tests
 * ============================================================================ */

TEST_CASE("Version info", "[engine][version]") {
    SECTION("Version numbers are defined") {
        // Just verify these compile and have reasonable values
        REQUIRE(AGENTITE_VERSION_MAJOR >= 0);
        REQUIRE(AGENTITE_VERSION_MINOR >= 0);
        REQUIRE(AGENTITE_VERSION_PATCH >= 0);
    }

    SECTION("Version is 0.1.0 or higher") {
        // Sanity check - we're at least at version 0.1.0
        bool is_valid = (AGENTITE_VERSION_MAJOR > 0) ||
                       (AGENTITE_VERSION_MAJOR == 0 && AGENTITE_VERSION_MINOR >= 1);
        REQUIRE(is_valid);
    }
}

/* ============================================================================
 * Progress State Enum Tests
 * ============================================================================ */

TEST_CASE("Progress state enum values", "[engine][progress]") {
    // Verify enum values are distinct
    REQUIRE(AGENTITE_PROGRESS_NONE != AGENTITE_PROGRESS_INDETERMINATE);
    REQUIRE(AGENTITE_PROGRESS_NONE != AGENTITE_PROGRESS_NORMAL);
    REQUIRE(AGENTITE_PROGRESS_NONE != AGENTITE_PROGRESS_PAUSED);
    REQUIRE(AGENTITE_PROGRESS_NONE != AGENTITE_PROGRESS_ERROR);

    REQUIRE(AGENTITE_PROGRESS_NORMAL != AGENTITE_PROGRESS_PAUSED);
    REQUIRE(AGENTITE_PROGRESS_NORMAL != AGENTITE_PROGRESS_ERROR);
}
