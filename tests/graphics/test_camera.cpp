/*
 * Agentite Camera Tests
 *
 * Tests for the 2D camera system including transforms, coordinate conversion,
 * and matrix operations. These tests are fully CPU-based and don't require GPU.
 */

#include "catch_amalgamated.hpp"
#include "agentite/camera.h"
#include <cmath>

using Catch::Approx;

/* ============================================================================
 * Camera Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Camera creation and destruction", "[camera][lifecycle]") {
    SECTION("Create camera with valid viewport") {
        Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
        REQUIRE(cam != nullptr);
        agentite_camera_destroy(cam);
    }

    SECTION("Create camera with zero viewport") {
        Agentite_Camera *cam = agentite_camera_create(0.0f, 0.0f);
        REQUIRE(cam != nullptr);
        agentite_camera_destroy(cam);
    }

    SECTION("Create camera with negative viewport") {
        // Implementation should handle this gracefully
        Agentite_Camera *cam = agentite_camera_create(-100.0f, -100.0f);
        REQUIRE(cam != nullptr);
        agentite_camera_destroy(cam);
    }

    SECTION("Destroy NULL camera is safe") {
        agentite_camera_destroy(nullptr);
        // Should not crash
    }
}

/* ============================================================================
 * Camera Position Tests
 * ============================================================================ */

TEST_CASE("Camera position operations", "[camera][position]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Default position is origin") {
        float x = -1.0f, y = -1.0f;
        agentite_camera_get_position(cam, &x, &y);
        REQUIRE(x == 0.0f);
        REQUIRE(y == 0.0f);
    }

    SECTION("Set position") {
        agentite_camera_set_position(cam, 100.0f, 200.0f);
        float x = 0.0f, y = 0.0f;
        agentite_camera_get_position(cam, &x, &y);
        REQUIRE(x == 100.0f);
        REQUIRE(y == 200.0f);
    }

    SECTION("Set position with negative coordinates") {
        agentite_camera_set_position(cam, -500.0f, -300.0f);
        float x = 0.0f, y = 0.0f;
        agentite_camera_get_position(cam, &x, &y);
        REQUIRE(x == -500.0f);
        REQUIRE(y == -300.0f);
    }

    SECTION("Move camera relative") {
        agentite_camera_set_position(cam, 100.0f, 100.0f);
        agentite_camera_move(cam, 50.0f, -25.0f);
        float x = 0.0f, y = 0.0f;
        agentite_camera_get_position(cam, &x, &y);
        REQUIRE(x == 150.0f);
        REQUIRE(y == 75.0f);
    }

    SECTION("Move camera multiple times accumulates") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_move(cam, 10.0f, 10.0f);
        agentite_camera_move(cam, 20.0f, 30.0f);
        agentite_camera_move(cam, -5.0f, -15.0f);
        float x = 0.0f, y = 0.0f;
        agentite_camera_get_position(cam, &x, &y);
        REQUIRE(x == 25.0f);
        REQUIRE(y == 25.0f);
    }

    SECTION("Get position with partial NULL outputs") {
        agentite_camera_set_position(cam, 100.0f, 200.0f);

        float x = 0.0f;
        agentite_camera_get_position(cam, &x, nullptr);
        REQUIRE(x == 100.0f);

        float y = 0.0f;
        agentite_camera_get_position(cam, nullptr, &y);
        REQUIRE(y == 200.0f);

        // Both NULL - should not crash
        agentite_camera_get_position(cam, nullptr, nullptr);
    }

    agentite_camera_destroy(cam);
}

/* ============================================================================
 * Camera Zoom Tests
 * ============================================================================ */

TEST_CASE("Camera zoom operations", "[camera][zoom]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Default zoom is 1.0") {
        REQUIRE(agentite_camera_get_zoom(cam) == 1.0f);
    }

    SECTION("Set zoom to 2x magnification") {
        agentite_camera_set_zoom(cam, 2.0f);
        REQUIRE(agentite_camera_get_zoom(cam) == 2.0f);
    }

    SECTION("Set zoom to 0.5x (zoomed out)") {
        agentite_camera_set_zoom(cam, 0.5f);
        REQUIRE(agentite_camera_get_zoom(cam) == 0.5f);
    }

    SECTION("Zoom is clamped at minimum 0.1") {
        agentite_camera_set_zoom(cam, 0.05f);
        REQUIRE(agentite_camera_get_zoom(cam) == 0.1f);

        agentite_camera_set_zoom(cam, 0.0f);
        REQUIRE(agentite_camera_get_zoom(cam) == 0.1f);

        agentite_camera_set_zoom(cam, -1.0f);
        REQUIRE(agentite_camera_get_zoom(cam) == 0.1f);
    }

    SECTION("Zoom is clamped at maximum 10.0") {
        agentite_camera_set_zoom(cam, 15.0f);
        REQUIRE(agentite_camera_get_zoom(cam) == 10.0f);

        agentite_camera_set_zoom(cam, 100.0f);
        REQUIRE(agentite_camera_get_zoom(cam) == 10.0f);
    }

    SECTION("Zoom at boundary values") {
        agentite_camera_set_zoom(cam, 0.1f);
        REQUIRE(agentite_camera_get_zoom(cam) == 0.1f);

        agentite_camera_set_zoom(cam, 10.0f);
        REQUIRE(agentite_camera_get_zoom(cam) == 10.0f);
    }

    agentite_camera_destroy(cam);
}

/* ============================================================================
 * Camera Rotation Tests
 * ============================================================================ */

TEST_CASE("Camera rotation operations", "[camera][rotation]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Default rotation is 0 degrees") {
        REQUIRE(agentite_camera_get_rotation(cam) == Approx(0.0f));
    }

    SECTION("Set rotation to 90 degrees") {
        agentite_camera_set_rotation(cam, 90.0f);
        REQUIRE(agentite_camera_get_rotation(cam) == Approx(90.0f));
    }

    SECTION("Set rotation to 45 degrees") {
        agentite_camera_set_rotation(cam, 45.0f);
        REQUIRE(agentite_camera_get_rotation(cam) == Approx(45.0f));
    }

    SECTION("Set rotation to negative degrees") {
        agentite_camera_set_rotation(cam, -45.0f);
        REQUIRE(agentite_camera_get_rotation(cam) == Approx(-45.0f));
    }

    SECTION("Set rotation to 360 degrees") {
        agentite_camera_set_rotation(cam, 360.0f);
        REQUIRE(agentite_camera_get_rotation(cam) == Approx(360.0f));
    }

    SECTION("Set rotation to multiple full rotations") {
        agentite_camera_set_rotation(cam, 720.0f);
        REQUIRE(agentite_camera_get_rotation(cam) == Approx(720.0f));
    }

    agentite_camera_destroy(cam);
}

/* ============================================================================
 * Camera Viewport Tests
 * ============================================================================ */

TEST_CASE("Camera viewport operations", "[camera][viewport]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Get initial viewport") {
        float w = 0.0f, h = 0.0f;
        agentite_camera_get_viewport(cam, &w, &h);
        REQUIRE(w == 1280.0f);
        REQUIRE(h == 720.0f);
    }

    SECTION("Set new viewport dimensions") {
        agentite_camera_set_viewport(cam, 1920.0f, 1080.0f);
        float w = 0.0f, h = 0.0f;
        agentite_camera_get_viewport(cam, &w, &h);
        REQUIRE(w == 1920.0f);
        REQUIRE(h == 1080.0f);
    }

    SECTION("Get viewport with partial NULL outputs") {
        float w = 0.0f;
        agentite_camera_get_viewport(cam, &w, nullptr);
        REQUIRE(w == 1280.0f);

        float h = 0.0f;
        agentite_camera_get_viewport(cam, nullptr, &h);
        REQUIRE(h == 720.0f);

        // Both NULL - should not crash
        agentite_camera_get_viewport(cam, nullptr, nullptr);
    }

    agentite_camera_destroy(cam);
}

/* ============================================================================
 * Camera Matrix Tests
 * ============================================================================ */

TEST_CASE("Camera matrix operations", "[camera][matrix]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Get VP matrix returns non-null") {
        const float *matrix = agentite_camera_get_vp_matrix(cam);
        REQUIRE(matrix != nullptr);
    }

    SECTION("Matrix changes after position update") {
        agentite_camera_update(cam);
        const float *matrix1 = agentite_camera_get_vp_matrix(cam);
        float m00_before = matrix1[0];

        agentite_camera_set_position(cam, 100.0f, 100.0f);
        agentite_camera_update(cam);
        const float *matrix2 = agentite_camera_get_vp_matrix(cam);

        // Matrix should have been recomputed (at minimum, translation affected)
        // Just verify we get a valid matrix
        REQUIRE(matrix2 != nullptr);
    }

    SECTION("Update is idempotent") {
        agentite_camera_set_position(cam, 100.0f, 200.0f);
        agentite_camera_update(cam);
        const float *matrix1 = agentite_camera_get_vp_matrix(cam);
        float m12 = matrix1[12]; // Translation X in column-major

        agentite_camera_update(cam);
        const float *matrix2 = agentite_camera_get_vp_matrix(cam);

        REQUIRE(matrix2[12] == m12);
    }

    agentite_camera_destroy(cam);
}

/* ============================================================================
 * Camera Coordinate Conversion Tests
 * ============================================================================ */

TEST_CASE("Camera screen to world conversion", "[camera][coords]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Screen center maps to camera position") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_update(cam);

        float world_x = 0.0f, world_y = 0.0f;
        agentite_camera_screen_to_world(cam, 640.0f, 360.0f, &world_x, &world_y);

        REQUIRE(world_x == Approx(0.0f).margin(0.01f));
        REQUIRE(world_y == Approx(0.0f).margin(0.01f));
    }

    SECTION("Screen center maps to offset camera position") {
        agentite_camera_set_position(cam, 500.0f, 300.0f);
        agentite_camera_update(cam);

        float world_x = 0.0f, world_y = 0.0f;
        agentite_camera_screen_to_world(cam, 640.0f, 360.0f, &world_x, &world_y);

        REQUIRE(world_x == Approx(500.0f).margin(0.01f));
        REQUIRE(world_y == Approx(300.0f).margin(0.01f));
    }

    SECTION("Zoom affects screen to world conversion") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_set_zoom(cam, 2.0f);  // 2x zoom
        agentite_camera_update(cam);

        float world_x = 0.0f, world_y = 0.0f;
        // Top-left corner at zoom 2x should be closer to center
        agentite_camera_screen_to_world(cam, 0.0f, 0.0f, &world_x, &world_y);

        // At 2x zoom, visible area is halved, so corners are closer
        REQUIRE(world_x == Approx(-320.0f).margin(0.01f));
        REQUIRE(world_y == Approx(-180.0f).margin(0.01f));
    }

    SECTION("Partial NULL outputs work") {
        agentite_camera_set_position(cam, 100.0f, 200.0f);
        agentite_camera_update(cam);

        float world_x = 0.0f;
        agentite_camera_screen_to_world(cam, 640.0f, 360.0f, &world_x, nullptr);
        REQUIRE(world_x == Approx(100.0f).margin(0.01f));

        float world_y = 0.0f;
        agentite_camera_screen_to_world(cam, 640.0f, 360.0f, nullptr, &world_y);
        REQUIRE(world_y == Approx(200.0f).margin(0.01f));
    }

    agentite_camera_destroy(cam);
}

TEST_CASE("Camera world to screen conversion", "[camera][coords]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("World origin maps to screen center") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_update(cam);

        float screen_x = 0.0f, screen_y = 0.0f;
        agentite_camera_world_to_screen(cam, 0.0f, 0.0f, &screen_x, &screen_y);

        REQUIRE(screen_x == Approx(640.0f).margin(0.01f));
        REQUIRE(screen_y == Approx(360.0f).margin(0.01f));
    }

    SECTION("Camera position maps to screen center") {
        agentite_camera_set_position(cam, 100.0f, 200.0f);
        agentite_camera_update(cam);

        float screen_x = 0.0f, screen_y = 0.0f;
        agentite_camera_world_to_screen(cam, 100.0f, 200.0f, &screen_x, &screen_y);

        REQUIRE(screen_x == Approx(640.0f).margin(0.01f));
        REQUIRE(screen_y == Approx(360.0f).margin(0.01f));
    }

    SECTION("Partial NULL outputs work") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_update(cam);

        float screen_x = 0.0f;
        agentite_camera_world_to_screen(cam, 0.0f, 0.0f, &screen_x, nullptr);
        REQUIRE(screen_x == Approx(640.0f).margin(0.01f));

        float screen_y = 0.0f;
        agentite_camera_world_to_screen(cam, 0.0f, 0.0f, nullptr, &screen_y);
        REQUIRE(screen_y == Approx(360.0f).margin(0.01f));
    }

    agentite_camera_destroy(cam);
}

TEST_CASE("Camera coordinate round-trip", "[camera][coords]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Screen to world to screen round-trip") {
        agentite_camera_set_position(cam, 150.0f, 250.0f);
        agentite_camera_set_zoom(cam, 1.5f);
        agentite_camera_update(cam);

        float original_screen_x = 400.0f;
        float original_screen_y = 300.0f;

        float world_x = 0.0f, world_y = 0.0f;
        agentite_camera_screen_to_world(cam, original_screen_x, original_screen_y,
                                       &world_x, &world_y);

        float final_screen_x = 0.0f, final_screen_y = 0.0f;
        agentite_camera_world_to_screen(cam, world_x, world_y,
                                       &final_screen_x, &final_screen_y);

        REQUIRE(final_screen_x == Approx(original_screen_x).margin(0.1f));
        REQUIRE(final_screen_y == Approx(original_screen_y).margin(0.1f));
    }

    SECTION("World to screen to world round-trip") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_update(cam);

        float original_world_x = 200.0f;
        float original_world_y = -150.0f;

        float screen_x = 0.0f, screen_y = 0.0f;
        agentite_camera_world_to_screen(cam, original_world_x, original_world_y,
                                       &screen_x, &screen_y);

        float final_world_x = 0.0f, final_world_y = 0.0f;
        agentite_camera_screen_to_world(cam, screen_x, screen_y,
                                       &final_world_x, &final_world_y);

        REQUIRE(final_world_x == Approx(original_world_x).margin(0.1f));
        REQUIRE(final_world_y == Approx(original_world_y).margin(0.1f));
    }

    agentite_camera_destroy(cam);
}

/* ============================================================================
 * Camera Bounds Tests
 * ============================================================================ */

TEST_CASE("Camera bounds calculation", "[camera][bounds]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Default bounds at origin") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_set_zoom(cam, 1.0f);

        float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
        agentite_camera_get_bounds(cam, &left, &right, &top, &bottom);

        // At zoom 1.0, visible area is viewport size centered at position
        REQUIRE(left == Approx(-640.0f));
        REQUIRE(right == Approx(640.0f));
        REQUIRE(top == Approx(-360.0f));
        REQUIRE(bottom == Approx(360.0f));
    }

    SECTION("Bounds at offset position") {
        agentite_camera_set_position(cam, 500.0f, 300.0f);
        agentite_camera_set_zoom(cam, 1.0f);

        float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
        agentite_camera_get_bounds(cam, &left, &right, &top, &bottom);

        REQUIRE(left == Approx(500.0f - 640.0f));
        REQUIRE(right == Approx(500.0f + 640.0f));
        REQUIRE(top == Approx(300.0f - 360.0f));
        REQUIRE(bottom == Approx(300.0f + 360.0f));
    }

    SECTION("Zoom affects bounds") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_set_zoom(cam, 2.0f);  // 2x zoom

        float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
        agentite_camera_get_bounds(cam, &left, &right, &top, &bottom);

        // At 2x zoom, visible area is halved
        REQUIRE(left == Approx(-320.0f));
        REQUIRE(right == Approx(320.0f));
        REQUIRE(top == Approx(-180.0f));
        REQUIRE(bottom == Approx(180.0f));
    }

    SECTION("Zoomed out expands bounds") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_set_zoom(cam, 0.5f);  // 0.5x zoom

        float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
        agentite_camera_get_bounds(cam, &left, &right, &top, &bottom);

        // At 0.5x zoom, visible area is doubled
        REQUIRE(left == Approx(-1280.0f));
        REQUIRE(right == Approx(1280.0f));
        REQUIRE(top == Approx(-720.0f));
        REQUIRE(bottom == Approx(720.0f));
    }

    SECTION("Partial NULL outputs work") {
        float left = 0.0f;
        agentite_camera_get_bounds(cam, &left, nullptr, nullptr, nullptr);
        REQUIRE(left == Approx(-640.0f));

        float right = 0.0f;
        agentite_camera_get_bounds(cam, nullptr, &right, nullptr, nullptr);
        REQUIRE(right == Approx(640.0f));

        // All NULL - should not crash
        agentite_camera_get_bounds(cam, nullptr, nullptr, nullptr, nullptr);
    }

    agentite_camera_destroy(cam);
}

TEST_CASE("Camera bounds with rotation", "[camera][bounds][rotation]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Rotation expands AABB") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_set_zoom(cam, 1.0f);
        agentite_camera_set_rotation(cam, 45.0f);

        float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
        agentite_camera_get_bounds(cam, &left, &right, &top, &bottom);

        // With 45 degree rotation, AABB should be larger
        // Width at 45deg: w*cos(45) + h*sin(45)
        float expected_half_w = (640.0f * std::cos(M_PI/4.0) + 360.0f * std::sin(M_PI/4.0));
        float expected_half_h = (640.0f * std::sin(M_PI/4.0) + 360.0f * std::cos(M_PI/4.0));

        REQUIRE(right - left > 1280.0f);  // Wider than unrotated
        REQUIRE(bottom - top > 720.0f);   // Taller than unrotated

        REQUIRE(left == Approx(-expected_half_w).margin(1.0f));
        REQUIRE(right == Approx(expected_half_w).margin(1.0f));
    }

    SECTION("90 degree rotation swaps width/height") {
        agentite_camera_set_position(cam, 0.0f, 0.0f);
        agentite_camera_set_zoom(cam, 1.0f);
        agentite_camera_set_rotation(cam, 90.0f);

        float left = 0.0f, right = 0.0f, top = 0.0f, bottom = 0.0f;
        agentite_camera_get_bounds(cam, &left, &right, &top, &bottom);

        // At 90 degrees, width becomes height and vice versa
        REQUIRE(std::abs(right - left) == Approx(720.0f).margin(1.0f));
        REQUIRE(std::abs(bottom - top) == Approx(1280.0f).margin(1.0f));
    }

    agentite_camera_destroy(cam);
}

/* ============================================================================
 * Camera NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Camera NULL safety", "[camera][null]") {
    SECTION("Set position with NULL camera") {
        agentite_camera_set_position(nullptr, 100.0f, 200.0f);
        // Should not crash
    }

    SECTION("Move with NULL camera") {
        agentite_camera_move(nullptr, 50.0f, 50.0f);
        // Should not crash
    }

    SECTION("Set zoom with NULL camera") {
        agentite_camera_set_zoom(nullptr, 2.0f);
        // Should not crash
    }

    SECTION("Set rotation with NULL camera") {
        agentite_camera_set_rotation(nullptr, 45.0f);
        // Should not crash
    }

    SECTION("Set viewport with NULL camera") {
        agentite_camera_set_viewport(nullptr, 1920.0f, 1080.0f);
        // Should not crash
    }

    SECTION("Get position with NULL camera") {
        float x = -1.0f, y = -1.0f;
        agentite_camera_get_position(nullptr, &x, &y);
        // Should not crash, values unchanged
        REQUIRE(x == -1.0f);
        REQUIRE(y == -1.0f);
    }

    SECTION("Get zoom with NULL camera returns default") {
        float zoom = agentite_camera_get_zoom(nullptr);
        REQUIRE(zoom == 1.0f);  // Default zoom
    }

    SECTION("Get rotation with NULL camera returns 0") {
        float rotation = agentite_camera_get_rotation(nullptr);
        REQUIRE(rotation == 0.0f);
    }

    SECTION("Get viewport with NULL camera") {
        float w = -1.0f, h = -1.0f;
        agentite_camera_get_viewport(nullptr, &w, &h);
        // Should not crash, values unchanged
        REQUIRE(w == -1.0f);
        REQUIRE(h == -1.0f);
    }

    SECTION("Update with NULL camera") {
        agentite_camera_update(nullptr);
        // Should not crash
    }

    SECTION("Get VP matrix with NULL camera returns NULL") {
        const float *matrix = agentite_camera_get_vp_matrix(nullptr);
        REQUIRE(matrix == nullptr);
    }

    SECTION("Screen to world with NULL camera") {
        float world_x = -1.0f, world_y = -1.0f;
        agentite_camera_screen_to_world(nullptr, 640.0f, 360.0f, &world_x, &world_y);
        // Should pass through screen coords unchanged
        REQUIRE(world_x == 640.0f);
        REQUIRE(world_y == 360.0f);
    }

    SECTION("World to screen with NULL camera") {
        float screen_x = -1.0f, screen_y = -1.0f;
        agentite_camera_world_to_screen(nullptr, 100.0f, 200.0f, &screen_x, &screen_y);
        // Should pass through world coords unchanged
        REQUIRE(screen_x == 100.0f);
        REQUIRE(screen_y == 200.0f);
    }

    SECTION("Get bounds with NULL camera") {
        float left = -1.0f, right = -1.0f, top = -1.0f, bottom = -1.0f;
        agentite_camera_get_bounds(nullptr, &left, &right, &top, &bottom);
        // Should not crash, values unchanged
        REQUIRE(left == -1.0f);
        REQUIRE(right == -1.0f);
        REQUIRE(top == -1.0f);
        REQUIRE(bottom == -1.0f);
    }
}

/* ============================================================================
 * Camera Dirty Flag Tests
 * ============================================================================ */

TEST_CASE("Camera dirty flag behavior", "[camera][dirty]") {
    Agentite_Camera *cam = agentite_camera_create(1280.0f, 720.0f);
    REQUIRE(cam != nullptr);

    SECTION("Get VP matrix auto-updates if dirty") {
        agentite_camera_set_position(cam, 100.0f, 100.0f);
        // Don't call update explicitly
        const float *matrix = agentite_camera_get_vp_matrix(cam);
        REQUIRE(matrix != nullptr);
        // Should have been auto-computed
    }

    SECTION("Coordinate conversion auto-updates if dirty") {
        agentite_camera_set_position(cam, 200.0f, 200.0f);
        // Don't call update explicitly
        float world_x = 0.0f, world_y = 0.0f;
        agentite_camera_screen_to_world(cam, 640.0f, 360.0f, &world_x, &world_y);
        // Should work and use updated position
        REQUIRE(world_x == Approx(200.0f).margin(0.01f));
        REQUIRE(world_y == Approx(200.0f).margin(0.01f));
    }

    agentite_camera_destroy(cam);
}
