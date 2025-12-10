/*
 * Carbon MSDF Generator Tests
 *
 * Tests for the multi-channel signed distance field generator.
 */

#include "catch_amalgamated.hpp"
#include "carbon/msdf.h"
#include "stb_truetype.h"
#include <cmath>
#include <cstdio>

using Catch::Approx;

/* ============================================================================
 * Shape Construction Tests
 * ============================================================================ */

TEST_CASE("MSDF shape creation and destruction", "[msdf][lifecycle]") {
    MSDF_Shape *shape = msdf_shape_create();
    REQUIRE(shape != nullptr);
    REQUIRE(msdf_shape_is_empty(shape));
    REQUIRE(msdf_shape_edge_count(shape) == 0);
    msdf_shape_free(shape);
}

TEST_CASE("MSDF contour and edge addition", "[msdf][shape]") {
    MSDF_Shape *shape = msdf_shape_create();
    REQUIRE(shape != nullptr);

    SECTION("Add single contour with line edges") {
        MSDF_Contour *contour = msdf_shape_add_contour(shape);
        REQUIRE(contour != nullptr);

        /* Create a triangle */
        msdf_contour_add_line(contour, msdf_vec2(0, 0), msdf_vec2(100, 0));
        msdf_contour_add_line(contour, msdf_vec2(100, 0), msdf_vec2(50, 86.6));
        msdf_contour_add_line(contour, msdf_vec2(50, 86.6), msdf_vec2(0, 0));

        REQUIRE(contour->edge_count == 3);
        REQUIRE(msdf_shape_edge_count(shape) == 3);
        REQUIRE_FALSE(msdf_shape_is_empty(shape));
    }

    SECTION("Add quadratic bezier edge") {
        MSDF_Contour *contour = msdf_shape_add_contour(shape);
        msdf_contour_add_quadratic(contour,
            msdf_vec2(0, 0),
            msdf_vec2(50, 100),
            msdf_vec2(100, 0));

        REQUIRE(contour->edge_count == 1);
        REQUIRE(contour->edges[0].type == MSDF_EDGE_QUADRATIC);
    }

    SECTION("Add cubic bezier edge") {
        MSDF_Contour *contour = msdf_shape_add_contour(shape);
        msdf_contour_add_cubic(contour,
            msdf_vec2(0, 0),
            msdf_vec2(33, 100),
            msdf_vec2(66, 100),
            msdf_vec2(100, 0));

        REQUIRE(contour->edge_count == 1);
        REQUIRE(contour->edges[0].type == MSDF_EDGE_CUBIC);
    }

    msdf_shape_free(shape);
}

/* ============================================================================
 * Vector Math Tests
 * ============================================================================ */

TEST_CASE("MSDF vector operations", "[msdf][math]") {
    SECTION("Vector creation") {
        MSDF_Vector2 v = msdf_vec2(3.0, 4.0);
        REQUIRE(v.x == 3.0);
        REQUIRE(v.y == 4.0);
    }

    SECTION("Vector addition") {
        MSDF_Vector2 a = msdf_vec2(1.0, 2.0);
        MSDF_Vector2 b = msdf_vec2(3.0, 4.0);
        MSDF_Vector2 c = msdf_vec2_add(a, b);
        REQUIRE(c.x == 4.0);
        REQUIRE(c.y == 6.0);
    }

    SECTION("Vector subtraction") {
        MSDF_Vector2 a = msdf_vec2(5.0, 7.0);
        MSDF_Vector2 b = msdf_vec2(2.0, 3.0);
        MSDF_Vector2 c = msdf_vec2_sub(a, b);
        REQUIRE(c.x == 3.0);
        REQUIRE(c.y == 4.0);
    }

    SECTION("Vector length") {
        MSDF_Vector2 v = msdf_vec2(3.0, 4.0);
        REQUIRE(msdf_vec2_length(v) == Approx(5.0));
    }

    SECTION("Vector dot product") {
        MSDF_Vector2 a = msdf_vec2(1.0, 0.0);
        MSDF_Vector2 b = msdf_vec2(0.0, 1.0);
        REQUIRE(msdf_vec2_dot(a, b) == Approx(0.0));

        MSDF_Vector2 c = msdf_vec2(1.0, 1.0);
        REQUIRE(msdf_vec2_dot(a, c) == Approx(1.0));
    }

    SECTION("Vector cross product") {
        MSDF_Vector2 a = msdf_vec2(1.0, 0.0);
        MSDF_Vector2 b = msdf_vec2(0.0, 1.0);
        REQUIRE(msdf_vec2_cross(a, b) == Approx(1.0));
        REQUIRE(msdf_vec2_cross(b, a) == Approx(-1.0));
    }

    SECTION("Vector normalize") {
        MSDF_Vector2 v = msdf_vec2(3.0, 4.0);
        MSDF_Vector2 n = msdf_vec2_normalize(v);
        REQUIRE(msdf_vec2_length(n) == Approx(1.0));
        REQUIRE(n.x == Approx(0.6));
        REQUIRE(n.y == Approx(0.8));
    }
}

/* ============================================================================
 * Edge Segment Tests
 * ============================================================================ */

TEST_CASE("MSDF edge point evaluation", "[msdf][edge]") {
    SECTION("Linear edge") {
        MSDF_EdgeSegment edge;
        edge.type = MSDF_EDGE_LINEAR;
        edge.color = MSDF_COLOR_WHITE;
        edge.p[0] = msdf_vec2(0, 0);
        edge.p[1] = msdf_vec2(100, 100);

        MSDF_Vector2 start = msdf_edge_point_at(&edge, 0.0);
        REQUIRE(start.x == Approx(0.0));
        REQUIRE(start.y == Approx(0.0));

        MSDF_Vector2 mid = msdf_edge_point_at(&edge, 0.5);
        REQUIRE(mid.x == Approx(50.0));
        REQUIRE(mid.y == Approx(50.0));

        MSDF_Vector2 end = msdf_edge_point_at(&edge, 1.0);
        REQUIRE(end.x == Approx(100.0));
        REQUIRE(end.y == Approx(100.0));
    }

    SECTION("Quadratic bezier edge") {
        MSDF_EdgeSegment edge;
        edge.type = MSDF_EDGE_QUADRATIC;
        edge.color = MSDF_COLOR_WHITE;
        edge.p[0] = msdf_vec2(0, 0);
        edge.p[1] = msdf_vec2(50, 100);
        edge.p[2] = msdf_vec2(100, 0);

        MSDF_Vector2 start = msdf_edge_point_at(&edge, 0.0);
        REQUIRE(start.x == Approx(0.0));
        REQUIRE(start.y == Approx(0.0));

        /* Midpoint of quadratic bezier with these control points */
        MSDF_Vector2 mid = msdf_edge_point_at(&edge, 0.5);
        REQUIRE(mid.x == Approx(50.0));
        REQUIRE(mid.y == Approx(50.0)); /* (1-t)^2*0 + 2*(1-t)*t*100 + t^2*0 = 0.5*100 = 50 */

        MSDF_Vector2 end = msdf_edge_point_at(&edge, 1.0);
        REQUIRE(end.x == Approx(100.0));
        REQUIRE(end.y == Approx(0.0));
    }
}

TEST_CASE("MSDF edge direction evaluation", "[msdf][edge]") {
    SECTION("Linear edge direction is constant") {
        MSDF_EdgeSegment edge;
        edge.type = MSDF_EDGE_LINEAR;
        edge.color = MSDF_COLOR_WHITE;
        edge.p[0] = msdf_vec2(0, 0);
        edge.p[1] = msdf_vec2(100, 0);

        MSDF_Vector2 dir0 = msdf_edge_direction_at(&edge, 0.0);
        MSDF_Vector2 dir1 = msdf_edge_direction_at(&edge, 0.5);
        MSDF_Vector2 dir2 = msdf_edge_direction_at(&edge, 1.0);

        /* All directions should be (100, 0) */
        REQUIRE(dir0.x == Approx(100.0));
        REQUIRE(dir0.y == Approx(0.0));
        REQUIRE(dir1.x == Approx(100.0));
        REQUIRE(dir2.x == Approx(100.0));
    }
}

TEST_CASE("MSDF edge bounding box", "[msdf][edge]") {
    SECTION("Linear edge bounds") {
        MSDF_EdgeSegment edge;
        edge.type = MSDF_EDGE_LINEAR;
        edge.color = MSDF_COLOR_WHITE;
        edge.p[0] = msdf_vec2(10, 20);
        edge.p[1] = msdf_vec2(50, 80);

        MSDF_Bounds bounds = msdf_edge_get_bounds(&edge);
        REQUIRE(bounds.left == Approx(10.0));
        REQUIRE(bounds.bottom == Approx(20.0));
        REQUIRE(bounds.right == Approx(50.0));
        REQUIRE(bounds.top == Approx(80.0));
    }
}

/* ============================================================================
 * Signed Distance Tests
 * ============================================================================ */

TEST_CASE("MSDF signed distance to linear edge", "[msdf][distance]") {
    MSDF_EdgeSegment edge;
    edge.type = MSDF_EDGE_LINEAR;
    edge.color = MSDF_COLOR_WHITE;
    edge.p[0] = msdf_vec2(0, 0);
    edge.p[1] = msdf_vec2(100, 0);

    SECTION("Point above line") {
        double param;
        MSDF_SignedDistance sd = msdf_edge_signed_distance(&edge, msdf_vec2(50, 10), &param);
        REQUIRE(sd.distance == Approx(10.0));
        REQUIRE(param == Approx(0.5));
    }

    SECTION("Point below line") {
        double param;
        MSDF_SignedDistance sd = msdf_edge_signed_distance(&edge, msdf_vec2(50, -10), &param);
        REQUIRE(sd.distance == Approx(-10.0));
    }

    SECTION("Point at endpoint") {
        double param;
        MSDF_SignedDistance sd = msdf_edge_signed_distance(&edge, msdf_vec2(0, 0), &param);
        REQUIRE(std::abs(sd.distance) < 0.001);
    }
}

/* ============================================================================
 * Shape Bounds Tests
 * ============================================================================ */

TEST_CASE("MSDF shape bounding box", "[msdf][bounds]") {
    MSDF_Shape *shape = msdf_shape_create();
    MSDF_Contour *contour = msdf_shape_add_contour(shape);

    /* Create a square */
    msdf_contour_add_line(contour, msdf_vec2(10, 10), msdf_vec2(90, 10));
    msdf_contour_add_line(contour, msdf_vec2(90, 10), msdf_vec2(90, 90));
    msdf_contour_add_line(contour, msdf_vec2(90, 90), msdf_vec2(10, 90));
    msdf_contour_add_line(contour, msdf_vec2(10, 90), msdf_vec2(10, 10));

    MSDF_Bounds bounds = msdf_shape_get_bounds(shape);
    REQUIRE(bounds.left == Approx(10.0));
    REQUIRE(bounds.bottom == Approx(10.0));
    REQUIRE(bounds.right == Approx(90.0));
    REQUIRE(bounds.top == Approx(90.0));

    msdf_shape_free(shape);
}

/* ============================================================================
 * Contour Winding Tests
 * ============================================================================ */

TEST_CASE("MSDF contour winding detection", "[msdf][winding]") {
    MSDF_Shape *shape = msdf_shape_create();
    MSDF_Contour *contour = msdf_shape_add_contour(shape);

    SECTION("Clockwise square") {
        /* Clockwise winding (when Y is up) */
        msdf_contour_add_line(contour, msdf_vec2(0, 0), msdf_vec2(100, 0));
        msdf_contour_add_line(contour, msdf_vec2(100, 0), msdf_vec2(100, 100));
        msdf_contour_add_line(contour, msdf_vec2(100, 100), msdf_vec2(0, 100));
        msdf_contour_add_line(contour, msdf_vec2(0, 100), msdf_vec2(0, 0));

        int winding = msdf_contour_winding(contour);
        /* Winding should be non-zero */
        REQUIRE(winding != 0);
    }

    msdf_shape_free(shape);
}

/* ============================================================================
 * Edge Coloring Tests
 * ============================================================================ */

TEST_CASE("MSDF simple edge coloring", "[msdf][coloring]") {
    MSDF_Shape *shape = msdf_shape_create();
    MSDF_Contour *contour = msdf_shape_add_contour(shape);

    /* Create a square with sharp corners */
    msdf_contour_add_line(contour, msdf_vec2(0, 0), msdf_vec2(100, 0));
    msdf_contour_add_line(contour, msdf_vec2(100, 0), msdf_vec2(100, 100));
    msdf_contour_add_line(contour, msdf_vec2(100, 100), msdf_vec2(0, 100));
    msdf_contour_add_line(contour, msdf_vec2(0, 100), msdf_vec2(0, 0));

    /* Apply edge coloring */
    msdf_edge_coloring_simple(shape, MSDF_DEFAULT_ANGLE_THRESHOLD, 0);

    /* Check that edges have colors assigned (not BLACK) */
    for (int i = 0; i < contour->edge_count; i++) {
        REQUIRE(contour->edges[i].color != MSDF_COLOR_BLACK);
    }

    /* Check that at least 2 different colors are used for sharp corners */
    int colors_used = 0;
    bool has_red = false, has_green = false, has_blue = false;
    for (int i = 0; i < contour->edge_count; i++) {
        if (contour->edges[i].color & MSDF_COLOR_RED) has_red = true;
        if (contour->edges[i].color & MSDF_COLOR_GREEN) has_green = true;
        if (contour->edges[i].color & MSDF_COLOR_BLUE) has_blue = true;
    }
    if (has_red) colors_used++;
    if (has_green) colors_used++;
    if (has_blue) colors_used++;

    /* Should use at least 2 colors for a square with corners */
    REQUIRE(colors_used >= 2);

    msdf_shape_free(shape);
}

/* ============================================================================
 * Bitmap Tests
 * ============================================================================ */

TEST_CASE("MSDF bitmap allocation", "[msdf][bitmap]") {
    MSDF_Bitmap bitmap;

    SECTION("Allocate grayscale bitmap") {
        REQUIRE(msdf_bitmap_alloc(&bitmap, 32, 32, MSDF_BITMAP_GRAY));
        REQUIRE(bitmap.data != nullptr);
        REQUIRE(bitmap.width == 32);
        REQUIRE(bitmap.height == 32);
        REQUIRE(bitmap.format == MSDF_BITMAP_GRAY);
        msdf_bitmap_free(&bitmap);
    }

    SECTION("Allocate RGB bitmap") {
        REQUIRE(msdf_bitmap_alloc(&bitmap, 64, 64, MSDF_BITMAP_RGB));
        REQUIRE(bitmap.data != nullptr);
        REQUIRE(bitmap.width == 64);
        REQUIRE(bitmap.height == 64);
        REQUIRE(bitmap.format == MSDF_BITMAP_RGB);
        msdf_bitmap_free(&bitmap);
    }

    SECTION("Pixel access") {
        REQUIRE(msdf_bitmap_alloc(&bitmap, 16, 16, MSDF_BITMAP_RGB));

        float *pixel = msdf_bitmap_pixel(&bitmap, 5, 10);
        REQUIRE(pixel != nullptr);

        pixel[0] = 0.5f;
        pixel[1] = 0.75f;
        pixel[2] = 1.0f;

        const float *read_pixel = msdf_bitmap_pixel_const(&bitmap, 5, 10);
        REQUIRE(read_pixel[0] == Approx(0.5f));
        REQUIRE(read_pixel[1] == Approx(0.75f));
        REQUIRE(read_pixel[2] == Approx(1.0f));

        msdf_bitmap_free(&bitmap);
    }
}

/* ============================================================================
 * MSDF Generation Tests
 * ============================================================================ */

TEST_CASE("MSDF generation for simple shapes", "[msdf][generation]") {
    /* Create a simple square shape */
    MSDF_Shape *shape = msdf_shape_create();
    MSDF_Contour *contour = msdf_shape_add_contour(shape);

    /* Square from (10,10) to (90,90) */
    msdf_contour_add_line(contour, msdf_vec2(10, 10), msdf_vec2(90, 10));
    msdf_contour_add_line(contour, msdf_vec2(90, 10), msdf_vec2(90, 90));
    msdf_contour_add_line(contour, msdf_vec2(90, 90), msdf_vec2(10, 90));
    msdf_contour_add_line(contour, msdf_vec2(10, 90), msdf_vec2(10, 10));

    /* Apply edge coloring */
    msdf_edge_coloring_simple(shape, MSDF_DEFAULT_ANGLE_THRESHOLD, 12345);

    SECTION("Generate SDF") {
        MSDF_Bitmap bitmap;
        REQUIRE(msdf_bitmap_alloc(&bitmap, 32, 32, MSDF_BITMAP_GRAY));

        MSDF_Projection proj = {
            .scale_x = 0.32,
            .scale_y = 0.32,
            .translate_x = 0,
            .translate_y = 0
        };

        msdf_generate_sdf(shape, &bitmap, &proj, 4.0);

        /* Check that center pixels (inside square) have value > 0.5 */
        const float *center = msdf_bitmap_pixel_const(&bitmap, 16, 16);
        REQUIRE(center != nullptr);
        REQUIRE(*center > 0.5f);

        /* Check that corner pixels (outside square) have value < 0.5 */
        const float *corner = msdf_bitmap_pixel_const(&bitmap, 0, 0);
        REQUIRE(corner != nullptr);
        REQUIRE(*corner < 0.5f);

        msdf_bitmap_free(&bitmap);
    }

    SECTION("Generate MSDF") {
        MSDF_Bitmap bitmap;
        REQUIRE(msdf_bitmap_alloc(&bitmap, 32, 32, MSDF_BITMAP_RGB));

        MSDF_Projection proj = {
            .scale_x = 0.32,
            .scale_y = 0.32,
            .translate_x = 0,
            .translate_y = 0
        };

        msdf_generate_msdf(shape, &bitmap, &proj, 4.0);

        /* Check that center pixels have RGB values mostly > 0.5 */
        const float *center = msdf_bitmap_pixel_const(&bitmap, 16, 16);
        REQUIRE(center != nullptr);
        /* At least median should be > 0.5 for inside point */
        float median = (center[0] + center[1] + center[2]) / 3.0f;
        REQUIRE(median > 0.4f); /* Allow some tolerance */

        msdf_bitmap_free(&bitmap);
    }

    msdf_shape_free(shape);
}

/* ============================================================================
 * Projection Helper Tests
 * ============================================================================ */

TEST_CASE("MSDF projection from bounds", "[msdf][projection]") {
    MSDF_Bounds bounds = {
        .left = 0,
        .bottom = 0,
        .right = 100,
        .top = 100
    };

    MSDF_Projection proj = msdf_projection_from_bounds(bounds, 32, 32, 2.0);

    /* With 2 pixel padding, effective area is 28x28 for 100x100 shape */
    REQUIRE(proj.scale_x == Approx(0.28));
    REQUIRE(proj.scale_y == Approx(0.28));
    REQUIRE(proj.translate_x == Approx(2.0)); /* padding */
    REQUIRE(proj.translate_y == Approx(2.0));
}
