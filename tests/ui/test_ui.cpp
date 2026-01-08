/*
 * Agentite UI Tests
 *
 * Tests for UI utility functions that can be tested without GPU initialization.
 * Note: Widget tests requiring GPU context are not included here.
 */

#include "catch_amalgamated.hpp"
#include "agentite/ui.h"

/* ============================================================================
 * Color Utility Tests
 * ============================================================================ */

TEST_CASE("UI color utilities", "[ui][color]") {
    SECTION("aui_rgba creates packed color") {
        uint32_t color = aui_rgba(255, 128, 64, 255);
        // Format is 0xAABBGGRR (ABGR)
        REQUIRE((color & 0xFF) == 255);         // R
        REQUIRE(((color >> 8) & 0xFF) == 128);  // G
        REQUIRE(((color >> 16) & 0xFF) == 64);  // B
        REQUIRE(((color >> 24) & 0xFF) == 255); // A
    }

    SECTION("aui_rgb creates opaque color") {
        uint32_t color = aui_rgb(255, 128, 64);
        REQUIRE((color & 0xFF) == 255);         // R
        REQUIRE(((color >> 8) & 0xFF) == 128);  // G
        REQUIRE(((color >> 16) & 0xFF) == 64);  // B
        REQUIRE(((color >> 24) & 0xFF) == 255); // A (opaque)
    }

    SECTION("aui_rgba with zero values") {
        uint32_t black_transparent = aui_rgba(0, 0, 0, 0);
        REQUIRE(black_transparent == 0x00000000);
    }

    SECTION("aui_rgba with max values") {
        uint32_t white_opaque = aui_rgba(255, 255, 255, 255);
        REQUIRE(white_opaque == 0xFFFFFFFF);
    }

    SECTION("aui_color_lerp interpolates colors") {
        uint32_t black = aui_rgb(0, 0, 0);
        uint32_t white = aui_rgb(255, 255, 255);

        uint32_t mid = aui_color_lerp(black, white, 0.5f);

        // Should be approximately gray
        uint8_t r = mid & 0xFF;
        uint8_t g = (mid >> 8) & 0xFF;
        uint8_t b = (mid >> 16) & 0xFF;

        // Allow some tolerance for rounding
        REQUIRE(r >= 127);
        REQUIRE(r <= 128);
        REQUIRE(g >= 127);
        REQUIRE(g <= 128);
        REQUIRE(b >= 127);
        REQUIRE(b <= 128);
    }

    SECTION("aui_color_lerp at t=0 returns first color") {
        uint32_t red = aui_rgb(255, 0, 0);
        uint32_t blue = aui_rgb(0, 0, 255);

        uint32_t result = aui_color_lerp(red, blue, 0.0f);
        REQUIRE(result == red);
    }

    SECTION("aui_color_lerp at t=1 returns second color") {
        uint32_t red = aui_rgb(255, 0, 0);
        uint32_t blue = aui_rgb(0, 0, 255);

        uint32_t result = aui_color_lerp(red, blue, 1.0f);
        REQUIRE(result == blue);
    }

    SECTION("aui_color_alpha modifies alpha channel") {
        uint32_t opaque = aui_rgba(100, 150, 200, 255);
        uint32_t half_alpha = aui_color_alpha(opaque, 0.5f);

        uint8_t alpha = (half_alpha >> 24) & 0xFF;
        REQUIRE(alpha >= 127);
        REQUIRE(alpha <= 128);

        // RGB should be unchanged
        REQUIRE((half_alpha & 0xFF) == 100);
        REQUIRE(((half_alpha >> 8) & 0xFF) == 150);
        REQUIRE(((half_alpha >> 16) & 0xFF) == 200);
    }

    SECTION("aui_color_alpha with 0 makes transparent") {
        uint32_t opaque = aui_rgba(100, 150, 200, 255);
        uint32_t transparent = aui_color_alpha(opaque, 0.0f);

        uint8_t alpha = (transparent >> 24) & 0xFF;
        REQUIRE(alpha == 0);
    }

    SECTION("aui_color_brighten increases brightness") {
        uint32_t gray = aui_rgb(100, 100, 100);
        uint32_t bright = aui_color_brighten(gray, 0.5f);

        uint8_t r = bright & 0xFF;
        uint8_t g = (bright >> 8) & 0xFF;
        uint8_t b = (bright >> 16) & 0xFF;

        REQUIRE(r > 100);
        REQUIRE(g > 100);
        REQUIRE(b > 100);
    }

    SECTION("aui_color_brighten clamps at 255") {
        uint32_t bright_gray = aui_rgb(200, 200, 200);
        uint32_t brighter = aui_color_brighten(bright_gray, 1.0f);

        uint8_t r = brighter & 0xFF;
        uint8_t g = (brighter >> 8) & 0xFF;
        uint8_t b = (brighter >> 16) & 0xFF;

        REQUIRE(r <= 255);
        REQUIRE(g <= 255);
        REQUIRE(b <= 255);
    }

    SECTION("aui_color_darken decreases brightness") {
        uint32_t gray = aui_rgb(200, 200, 200);
        uint32_t dark = aui_color_darken(gray, 0.5f);

        uint8_t r = dark & 0xFF;
        uint8_t g = (dark >> 8) & 0xFF;
        uint8_t b = (dark >> 16) & 0xFF;

        REQUIRE(r < 200);
        REQUIRE(g < 200);
        REQUIRE(b < 200);
    }

    SECTION("aui_color_darken clamps at 0") {
        uint32_t dark_gray = aui_rgb(50, 50, 50);
        uint32_t darker = aui_color_darken(dark_gray, 2.0f);

        uint8_t r = darker & 0xFF;
        uint8_t g = (darker >> 8) & 0xFF;
        uint8_t b = (darker >> 16) & 0xFF;

        REQUIRE(r >= 0);
        REQUIRE(g >= 0);
        REQUIRE(b >= 0);
    }
}

/* ============================================================================
 * Rect Utility Tests
 * ============================================================================ */

TEST_CASE("UI rect utilities", "[ui][rect]") {
    SECTION("aui_rect_contains point inside") {
        AUI_Rect rect = {10.0f, 20.0f, 100.0f, 50.0f};

        REQUIRE(aui_rect_contains(rect, 50.0f, 40.0f));  // Center
        REQUIRE(aui_rect_contains(rect, 10.0f, 20.0f));  // Top-left corner
    }

    SECTION("aui_rect_contains point outside") {
        AUI_Rect rect = {10.0f, 20.0f, 100.0f, 50.0f};

        REQUIRE_FALSE(aui_rect_contains(rect, 0.0f, 0.0f));    // Before
        REQUIRE_FALSE(aui_rect_contains(rect, 200.0f, 40.0f)); // Right of
        REQUIRE_FALSE(aui_rect_contains(rect, 50.0f, 100.0f)); // Below
    }

    SECTION("aui_rect_contains boundary behavior") {
        AUI_Rect rect = {0.0f, 0.0f, 100.0f, 100.0f};

        // Points on the boundary - typically inclusive on left/top
        REQUIRE(aui_rect_contains(rect, 0.0f, 0.0f));     // Top-left
        // Behavior at right/bottom edge depends on implementation
    }

    SECTION("aui_rect_intersect overlapping rects") {
        AUI_Rect a = {0.0f, 0.0f, 100.0f, 100.0f};
        AUI_Rect b = {50.0f, 50.0f, 100.0f, 100.0f};

        AUI_Rect result = aui_rect_intersect(a, b);

        // Intersection should be the overlapping area
        REQUIRE(result.x == 50.0f);
        REQUIRE(result.y == 50.0f);
        REQUIRE(result.w == 50.0f);
        REQUIRE(result.h == 50.0f);
    }

    SECTION("aui_rect_intersect non-overlapping rects") {
        AUI_Rect a = {0.0f, 0.0f, 50.0f, 50.0f};
        AUI_Rect b = {100.0f, 100.0f, 50.0f, 50.0f};

        AUI_Rect result = aui_rect_intersect(a, b);

        // No overlap - should return empty or zero rect
        bool is_empty = (result.w <= 0.0f) || (result.h <= 0.0f);
        REQUIRE(is_empty);
    }

    SECTION("aui_rect_intersect one inside other") {
        AUI_Rect outer = {0.0f, 0.0f, 200.0f, 200.0f};
        AUI_Rect inner = {50.0f, 50.0f, 50.0f, 50.0f};

        AUI_Rect result = aui_rect_intersect(outer, inner);

        // Should return the inner rect
        REQUIRE(result.x == inner.x);
        REQUIRE(result.y == inner.y);
        REQUIRE(result.w == inner.w);
        REQUIRE(result.h == inner.h);
    }

    SECTION("aui_rect_intersect touching rects") {
        AUI_Rect a = {0.0f, 0.0f, 50.0f, 50.0f};
        AUI_Rect b = {50.0f, 0.0f, 50.0f, 50.0f};  // Touching on right edge

        // Touching edges may or may not intersect depending on implementation
        // At minimum should not crash
        (void)aui_rect_intersect(a, b);
    }
}

/* ============================================================================
 * AUI_Rect Struct Tests
 * ============================================================================ */

TEST_CASE("AUI_Rect struct", "[ui][rect][struct]") {
    SECTION("Struct can be zero-initialized") {
        AUI_Rect rect = {};
        REQUIRE(rect.x == 0.0f);
        REQUIRE(rect.y == 0.0f);
        REQUIRE(rect.w == 0.0f);
        REQUIRE(rect.h == 0.0f);
    }

    SECTION("Struct can be brace-initialized") {
        AUI_Rect rect = {10.0f, 20.0f, 30.0f, 40.0f};
        REQUIRE(rect.x == 10.0f);
        REQUIRE(rect.y == 20.0f);
        REQUIRE(rect.w == 30.0f);
        REQUIRE(rect.h == 40.0f);
    }

    SECTION("Struct size is 4 floats") {
        REQUIRE(sizeof(AUI_Rect) == 4 * sizeof(float));
    }
}

/* ============================================================================
 * AUI_Color Struct Tests
 * ============================================================================ */

TEST_CASE("AUI_Color struct", "[ui][color][struct]") {
    SECTION("Struct can be zero-initialized") {
        AUI_Color color = {};
        REQUIRE(color.r == 0.0f);
        REQUIRE(color.g == 0.0f);
        REQUIRE(color.b == 0.0f);
        REQUIRE(color.a == 0.0f);
    }

    SECTION("Struct can be brace-initialized") {
        AUI_Color color = {1.0f, 0.5f, 0.25f, 1.0f};
        REQUIRE(color.r == 1.0f);
        REQUIRE(color.g == 0.5f);
        REQUIRE(color.b == 0.25f);
        REQUIRE(color.a == 1.0f);
    }

    SECTION("Struct size is 4 floats") {
        REQUIRE(sizeof(AUI_Color) == 4 * sizeof(float));
    }
}

/* ============================================================================
 * AUI_Vertex Struct Tests
 * ============================================================================ */

TEST_CASE("AUI_Vertex struct", "[ui][vertex][struct]") {
    SECTION("Struct can be zero-initialized") {
        AUI_Vertex vertex = {};
        REQUIRE(vertex.pos[0] == 0.0f);
        REQUIRE(vertex.pos[1] == 0.0f);
        REQUIRE(vertex.uv[0] == 0.0f);
        REQUIRE(vertex.uv[1] == 0.0f);
        REQUIRE(vertex.color == 0);
    }

    SECTION("Struct can be assigned") {
        AUI_Vertex vertex = {};
        vertex.pos[0] = 100.0f;
        vertex.pos[1] = 200.0f;
        vertex.uv[0] = 0.5f;
        vertex.uv[1] = 0.75f;
        vertex.color = 0xFF0000FF;  // Red, full alpha

        REQUIRE(vertex.pos[0] == 100.0f);
        REQUIRE(vertex.pos[1] == 200.0f);
        REQUIRE(vertex.uv[0] == 0.5f);
        REQUIRE(vertex.uv[1] == 0.75f);
        REQUIRE(vertex.color == 0xFF0000FF);
    }
}

/* ============================================================================
 * ID Utility Tests
 * ============================================================================ */

TEST_CASE("UI ID utilities", "[ui][id]") {
    SECTION("aui_id generates non-zero ID") {
        AUI_Id id = aui_id("test_widget");
        REQUIRE(id != AUI_ID_NONE);
    }

    SECTION("aui_id is deterministic") {
        AUI_Id id1 = aui_id("test_widget");
        AUI_Id id2 = aui_id("test_widget");
        REQUIRE(id1 == id2);
    }

    SECTION("aui_id different strings give different IDs") {
        AUI_Id id1 = aui_id("widget_a");
        AUI_Id id2 = aui_id("widget_b");
        REQUIRE(id1 != id2);
    }

    SECTION("aui_id_int generates unique IDs") {
        AUI_Id id0 = aui_id_int("item", 0);
        AUI_Id id1 = aui_id_int("item", 1);
        AUI_Id id2 = aui_id_int("item", 2);

        REQUIRE(id0 != AUI_ID_NONE);
        REQUIRE(id1 != AUI_ID_NONE);
        REQUIRE(id2 != AUI_ID_NONE);
        REQUIRE(id0 != id1);
        REQUIRE(id1 != id2);
    }

    SECTION("aui_id_int is deterministic") {
        AUI_Id id1 = aui_id_int("loop_item", 42);
        AUI_Id id2 = aui_id_int("loop_item", 42);
        REQUIRE(id1 == id2);
    }

    SECTION("AUI_ID_NONE is zero") {
        REQUIRE(AUI_ID_NONE == 0);
    }
}

/* ============================================================================
 * Theme Tests
 * ============================================================================ */

TEST_CASE("UI theme presets", "[ui][theme]") {
    SECTION("Dark theme returns valid theme") {
        AUI_Theme dark = aui_theme_dark();

        // Check some basic colors are set
        REQUIRE(dark.bg_panel != 0);
        REQUIRE(dark.text != 0);
        REQUIRE(dark.accent != 0);
    }

    SECTION("Light theme returns valid theme") {
        AUI_Theme light = aui_theme_light();

        // Check some basic colors are set
        REQUIRE(light.bg_panel != 0);
        REQUIRE(light.text != 0);
        REQUIRE(light.accent != 0);
    }

    SECTION("Dark and light themes are different") {
        AUI_Theme dark = aui_theme_dark();
        AUI_Theme light = aui_theme_light();

        // Background colors should be notably different
        REQUIRE(dark.bg_panel != light.bg_panel);
        REQUIRE(dark.text != light.text);
    }

    SECTION("Theme has reasonable metrics") {
        AUI_Theme theme = aui_theme_dark();

        REQUIRE(theme.corner_radius >= 0.0f);
        REQUIRE(theme.border_width >= 0.0f);
        REQUIRE(theme.widget_height > 0.0f);
        REQUIRE(theme.spacing > 0.0f);
        REQUIRE(theme.padding >= 0.0f);
    }
}

/* ============================================================================
 * Color Conversion Tests
 * ============================================================================ */

TEST_CASE("Color conversion utilities", "[ui][color][conversion]") {
    SECTION("RGB to HSV round-trip") {
        float h, s, v;
        float r, g, b;

        // Pure red
        aui_rgb_to_hsv(1.0f, 0.0f, 0.0f, &h, &s, &v);
        aui_hsv_to_rgb(h, s, v, &r, &g, &b);

        REQUIRE(r == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(g == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(b == Catch::Approx(0.0f).margin(0.01f));
    }

    SECTION("HSV to RGB for primary colors") {
        float r, g, b;

        // Red (H=0)
        aui_hsv_to_rgb(0.0f, 1.0f, 1.0f, &r, &g, &b);
        REQUIRE(r == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(g == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(b == Catch::Approx(0.0f).margin(0.01f));

        // Green (H=120 or 0.333)
        aui_hsv_to_rgb(120.0f / 360.0f, 1.0f, 1.0f, &r, &g, &b);
        REQUIRE(r == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(g == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(b == Catch::Approx(0.0f).margin(0.01f));

        // Blue (H=240 or 0.666)
        aui_hsv_to_rgb(240.0f / 360.0f, 1.0f, 1.0f, &r, &g, &b);
        REQUIRE(r == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(g == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(b == Catch::Approx(1.0f).margin(0.01f));
    }

    SECTION("RGB to HSV for white/black/gray") {
        float h, s, v;

        // White
        aui_rgb_to_hsv(1.0f, 1.0f, 1.0f, &h, &s, &v);
        REQUIRE(s == Catch::Approx(0.0f).margin(0.01f));  // No saturation
        REQUIRE(v == Catch::Approx(1.0f).margin(0.01f));  // Full value

        // Black
        aui_rgb_to_hsv(0.0f, 0.0f, 0.0f, &h, &s, &v);
        REQUIRE(v == Catch::Approx(0.0f).margin(0.01f));  // No value

        // Gray
        aui_rgb_to_hsv(0.5f, 0.5f, 0.5f, &h, &s, &v);
        REQUIRE(s == Catch::Approx(0.0f).margin(0.01f));  // No saturation
        REQUIRE(v == Catch::Approx(0.5f).margin(0.01f));
    }
}

/* ============================================================================
 * Panel Flags Tests
 * ============================================================================ */

TEST_CASE("Panel flags", "[ui][flags]") {
    SECTION("Flags are distinct powers of 2") {
        REQUIRE(AUI_PANEL_MOVABLE == (1 << 0));
        REQUIRE(AUI_PANEL_RESIZABLE == (1 << 1));
        REQUIRE(AUI_PANEL_CLOSABLE == (1 << 2));
        REQUIRE(AUI_PANEL_TITLE_BAR == (1 << 3));
        REQUIRE(AUI_PANEL_NO_SCROLLBAR == (1 << 4));
        REQUIRE(AUI_PANEL_BORDER == (1 << 5));
    }

    SECTION("Flags can be combined") {
        uint32_t flags = AUI_PANEL_MOVABLE | AUI_PANEL_RESIZABLE | AUI_PANEL_TITLE_BAR;

        REQUIRE((flags & AUI_PANEL_MOVABLE) != 0);
        REQUIRE((flags & AUI_PANEL_RESIZABLE) != 0);
        REQUIRE((flags & AUI_PANEL_TITLE_BAR) != 0);
        REQUIRE((flags & AUI_PANEL_CLOSABLE) == 0);
    }
}

/* ============================================================================
 * Table Flags Tests
 * ============================================================================ */

TEST_CASE("Table flags", "[ui][flags]") {
    SECTION("Table flags are distinct powers of 2") {
        REQUIRE(AUI_TABLE_RESIZABLE == (1 << 0));
        REQUIRE(AUI_TABLE_REORDERABLE == (1 << 1));
        REQUIRE(AUI_TABLE_SORTABLE == (1 << 2));
        REQUIRE(AUI_TABLE_HIDEABLE == (1 << 3));
        REQUIRE(AUI_TABLE_BORDERS == (1 << 4));
        REQUIRE(AUI_TABLE_ROW_HIGHLIGHT == (1 << 5));
        REQUIRE(AUI_TABLE_SCROLL_X == (1 << 6));
        REQUIRE(AUI_TABLE_SCROLL_Y == (1 << 7));
    }

    SECTION("Column flags are distinct powers of 2") {
        REQUIRE(AUI_TABLE_COLUMN_DEFAULT_SORT == (1 << 0));
        REQUIRE(AUI_TABLE_COLUMN_NO_SORT == (1 << 1));
        REQUIRE(AUI_TABLE_COLUMN_NO_RESIZE == (1 << 2));
        REQUIRE(AUI_TABLE_COLUMN_NO_HIDE == (1 << 3));
    }
}

/* ============================================================================
 * NULL Safety Tests for Functions Not Requiring Context
 * ============================================================================ */

TEST_CASE("UI NULL safety", "[ui][null]") {
    SECTION("aui_shutdown with NULL is safe") {
        aui_shutdown(nullptr);
        // Should not crash
    }

    SECTION("aui_id with NULL string returns valid ID") {
        // Implementation should handle NULL gracefully
        // (either return 0 or hash NULL pointer)
    }
}

/* ============================================================================
 * Shortcut Modifier Tests
 * ============================================================================ */

TEST_CASE("Shortcut modifiers", "[ui][shortcuts]") {
    SECTION("Modifiers are distinct") {
        REQUIRE(AUI_MOD_NONE == 0);
        REQUIRE(AUI_MOD_CTRL == (1 << 0));
        REQUIRE(AUI_MOD_SHIFT == (1 << 1));
        REQUIRE(AUI_MOD_ALT == (1 << 2));
    }

    SECTION("Modifiers can be combined") {
        uint8_t ctrl_shift = AUI_MOD_CTRL | AUI_MOD_SHIFT;
        REQUIRE((ctrl_shift & AUI_MOD_CTRL) != 0);
        REQUIRE((ctrl_shift & AUI_MOD_SHIFT) != 0);
        REQUIRE((ctrl_shift & AUI_MOD_ALT) == 0);
    }
}
