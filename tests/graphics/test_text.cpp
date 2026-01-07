/*
 * Agentite Text Rendering Tests
 *
 * Tests for text rendering functionality that can be tested without GPU.
 * Note: Most text functions require the TextRenderer (GPU-dependent),
 * so these tests focus on NULL safety, enums, and struct defaults.
 */

#include "catch_amalgamated.hpp"
#include "agentite/text.h"
#include <cstring>

/* ============================================================================
 * Enum Tests
 * ============================================================================ */

TEST_CASE("Text alignment enum values", "[text][enum]") {
    SECTION("Alignment values are distinct") {
        REQUIRE(AGENTITE_TEXT_ALIGN_LEFT != AGENTITE_TEXT_ALIGN_CENTER);
        REQUIRE(AGENTITE_TEXT_ALIGN_LEFT != AGENTITE_TEXT_ALIGN_RIGHT);
        REQUIRE(AGENTITE_TEXT_ALIGN_CENTER != AGENTITE_TEXT_ALIGN_RIGHT);
    }

    SECTION("Default alignment is left") {
        REQUIRE(static_cast<int>(AGENTITE_TEXT_ALIGN_LEFT) == 0);
    }

    SECTION("Alignment values are sequential") {
        REQUIRE(static_cast<int>(AGENTITE_TEXT_ALIGN_LEFT) == 0);
        REQUIRE(static_cast<int>(AGENTITE_TEXT_ALIGN_CENTER) == 1);
        REQUIRE(static_cast<int>(AGENTITE_TEXT_ALIGN_RIGHT) == 2);
    }
}

TEST_CASE("SDF font type enum values", "[text][sdf][enum]") {
    SECTION("SDF types are distinct") {
        REQUIRE(AGENTITE_SDF_TYPE_SDF != AGENTITE_SDF_TYPE_MSDF);
    }

    SECTION("SDF type values") {
        REQUIRE(static_cast<int>(AGENTITE_SDF_TYPE_SDF) == 0);
        REQUIRE(static_cast<int>(AGENTITE_SDF_TYPE_MSDF) == 1);
    }
}

/* ============================================================================
 * Text Effects Struct Tests
 * ============================================================================ */

TEST_CASE("TextEffects struct", "[text][effects]") {
    SECTION("Zero-initialized effects are disabled") {
        Agentite_TextEffects effects = {};

        REQUIRE_FALSE(effects.outline_enabled);
        REQUIRE_FALSE(effects.shadow_enabled);
        REQUIRE_FALSE(effects.glow_enabled);
        REQUIRE(effects.outline_width == 0.0f);
        REQUIRE(effects.shadow_softness == 0.0f);
        REQUIRE(effects.glow_width == 0.0f);
        REQUIRE(effects.weight == 0.0f);
    }

    SECTION("Effects can be configured") {
        Agentite_TextEffects effects = {};

        // Configure outline
        effects.outline_enabled = true;
        effects.outline_width = 0.3f;
        effects.outline_color[0] = 0.0f;
        effects.outline_color[1] = 0.0f;
        effects.outline_color[2] = 0.0f;
        effects.outline_color[3] = 1.0f;

        REQUIRE(effects.outline_enabled);
        REQUIRE(effects.outline_width == 0.3f);
        REQUIRE(effects.outline_color[3] == 1.0f);
    }

    SECTION("Shadow effect fields") {
        Agentite_TextEffects effects = {};

        effects.shadow_enabled = true;
        effects.shadow_offset[0] = 2.0f;
        effects.shadow_offset[1] = 2.0f;
        effects.shadow_softness = 0.5f;
        effects.shadow_color[0] = 0.0f;
        effects.shadow_color[1] = 0.0f;
        effects.shadow_color[2] = 0.0f;
        effects.shadow_color[3] = 0.7f;

        REQUIRE(effects.shadow_enabled);
        REQUIRE(effects.shadow_offset[0] == 2.0f);
        REQUIRE(effects.shadow_offset[1] == 2.0f);
        REQUIRE(effects.shadow_softness == 0.5f);
        REQUIRE(effects.shadow_color[3] == 0.7f);
    }

    SECTION("Glow effect fields") {
        Agentite_TextEffects effects = {};

        effects.glow_enabled = true;
        effects.glow_width = 0.25f;
        effects.glow_color[0] = 1.0f;
        effects.glow_color[1] = 1.0f;
        effects.glow_color[2] = 0.0f;
        effects.glow_color[3] = 1.0f;

        REQUIRE(effects.glow_enabled);
        REQUIRE(effects.glow_width == 0.25f);
        REQUIRE(effects.glow_color[0] == 1.0f);
    }

    SECTION("Weight adjustment") {
        Agentite_TextEffects effects = {};

        effects.weight = 0.2f;  // Slightly bold
        REQUIRE(effects.weight == 0.2f);

        effects.weight = -0.3f;  // Thinner
        REQUIRE(effects.weight == -0.3f);
    }
}

/* ============================================================================
 * SDF Font Gen Config Tests
 * ============================================================================ */

TEST_CASE("SDFFontGenConfig struct", "[text][sdf][config]") {
    SECTION("Default config macro") {
        Agentite_SDFFontGenConfig config = AGENTITE_SDF_FONT_GEN_CONFIG_DEFAULT;

        REQUIRE(config.atlas_width == 1024);
        REQUIRE(config.atlas_height == 1024);
        REQUIRE(config.glyph_scale == 48.0f);
        REQUIRE(config.pixel_range == 4.0f);
        REQUIRE(config.generate_msdf == true);
        REQUIRE(config.charset == nullptr);
    }

    SECTION("Custom config") {
        Agentite_SDFFontGenConfig config = {};
        config.atlas_width = 2048;
        config.atlas_height = 2048;
        config.glyph_scale = 64.0f;
        config.pixel_range = 8.0f;
        config.generate_msdf = false;
        config.charset = "ABC123";

        REQUIRE(config.atlas_width == 2048);
        REQUIRE(config.atlas_height == 2048);
        REQUIRE(config.glyph_scale == 64.0f);
        REQUIRE(config.pixel_range == 8.0f);
        REQUIRE_FALSE(config.generate_msdf);
        REQUIRE(std::strcmp(config.charset, "ABC123") == 0);
    }
}

/* ============================================================================
 * Text Renderer NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Text renderer NULL safety", "[text][renderer][null]") {
    SECTION("agentite_text_shutdown with NULL") {
        // Should not crash
        agentite_text_shutdown(nullptr);
    }

    SECTION("agentite_text_set_screen_size with NULL") {
        // Should not crash
        agentite_text_set_screen_size(nullptr, 1920, 1080);
    }

    SECTION("agentite_text_begin with NULL") {
        // Should not crash
        agentite_text_begin(nullptr);
    }

    SECTION("agentite_text_end with NULL") {
        // Should not crash
        agentite_text_end(nullptr);
    }

    SECTION("agentite_text_upload with NULL") {
        // Should not crash
        agentite_text_upload(nullptr, nullptr);
    }

    SECTION("agentite_text_render with NULL") {
        // Should not crash
        agentite_text_render(nullptr, nullptr, nullptr);
    }
}

/* ============================================================================
 * Font Loading NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Font loading NULL safety", "[text][font][null]") {
    SECTION("agentite_font_load with NULL renderer") {
        Agentite_Font *font = agentite_font_load(nullptr, "font.ttf", 24.0f);
        REQUIRE(font == nullptr);
    }

    SECTION("agentite_font_load with NULL path") {
        Agentite_Font *font = agentite_font_load(nullptr, nullptr, 24.0f);
        REQUIRE(font == nullptr);
    }

    SECTION("agentite_font_load_memory with NULL renderer") {
        const char data[] = "fake font data";
        Agentite_Font *font = agentite_font_load_memory(nullptr, data, sizeof(data), 24.0f);
        REQUIRE(font == nullptr);
    }

    SECTION("agentite_font_load_memory with NULL data") {
        Agentite_Font *font = agentite_font_load_memory(nullptr, nullptr, 100, 24.0f);
        REQUIRE(font == nullptr);
    }

    SECTION("agentite_font_destroy with NULL") {
        // Should not crash
        agentite_font_destroy(nullptr, nullptr);
    }
}

/* ============================================================================
 * Font Metrics NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Font metrics NULL safety", "[text][font][null]") {
    SECTION("agentite_font_get_size with NULL") {
        float size = agentite_font_get_size(nullptr);
        REQUIRE(size == 0.0f);
    }

    SECTION("agentite_font_get_line_height with NULL") {
        float height = agentite_font_get_line_height(nullptr);
        REQUIRE(height == 0.0f);
    }

    SECTION("agentite_font_get_ascent with NULL") {
        float ascent = agentite_font_get_ascent(nullptr);
        REQUIRE(ascent == 0.0f);
    }

    SECTION("agentite_font_get_descent with NULL") {
        float descent = agentite_font_get_descent(nullptr);
        REQUIRE(descent == 0.0f);
    }
}

/* ============================================================================
 * Text Measurement NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Text measurement NULL safety", "[text][measure][null]") {
    SECTION("agentite_text_measure with NULL font") {
        float width = agentite_text_measure(nullptr, "Hello");
        REQUIRE(width == 0.0f);
    }

    SECTION("agentite_text_measure with NULL text") {
        float width = agentite_text_measure(nullptr, nullptr);
        REQUIRE(width == 0.0f);
    }

    SECTION("agentite_text_measure_bounds with NULL font") {
        float w = -1.0f, h = -1.0f;
        agentite_text_measure_bounds(nullptr, "Hello", &w, &h);
        // Should set to 0 or leave unchanged
        REQUIRE(w >= 0.0f);  // Either 0 or unchanged
    }

    SECTION("agentite_text_measure_bounds with NULL text") {
        float w = -1.0f, h = -1.0f;
        agentite_text_measure_bounds(nullptr, nullptr, &w, &h);
        // Should not crash
    }
}

/* ============================================================================
 * Text Drawing NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Text drawing NULL safety", "[text][draw][null]") {
    SECTION("agentite_text_draw with NULL renderer") {
        // Should not crash
        agentite_text_draw(nullptr, nullptr, "Hello", 100.0f, 200.0f);
    }

    SECTION("agentite_text_draw_colored with NULL renderer") {
        // Should not crash
        agentite_text_draw_colored(nullptr, nullptr, "Hello", 100.0f, 200.0f,
                                  1.0f, 0.0f, 0.0f, 1.0f);
    }

    SECTION("agentite_text_draw_scaled with NULL renderer") {
        // Should not crash
        agentite_text_draw_scaled(nullptr, nullptr, "Hello", 100.0f, 200.0f, 2.0f);
    }

    SECTION("agentite_text_draw_ex with NULL renderer") {
        // Should not crash
        agentite_text_draw_ex(nullptr, nullptr, "Hello", 100.0f, 200.0f,
                            1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                            AGENTITE_TEXT_ALIGN_CENTER);
    }

    SECTION("agentite_text_printf with NULL renderer") {
        // Should not crash
        agentite_text_printf(nullptr, nullptr, 100.0f, 200.0f, "Score: %d", 42);
    }

    SECTION("agentite_text_printf_colored with NULL renderer") {
        // Should not crash
        agentite_text_printf_colored(nullptr, nullptr, 100.0f, 200.0f,
                                    1.0f, 0.0f, 0.0f, 1.0f, "Score: %d", 42);
    }
}

/* ============================================================================
 * SDF Font NULL Safety Tests
 * ============================================================================ */

TEST_CASE("SDF font NULL safety", "[text][sdf][null]") {
    SECTION("agentite_sdf_font_load with NULL renderer") {
        Agentite_SDFFont *font = agentite_sdf_font_load(nullptr, "atlas.png", "metrics.json");
        REQUIRE(font == nullptr);
    }

    SECTION("agentite_sdf_font_load with NULL paths") {
        Agentite_SDFFont *font = agentite_sdf_font_load(nullptr, nullptr, nullptr);
        REQUIRE(font == nullptr);
    }

    SECTION("agentite_sdf_font_generate with NULL renderer") {
        Agentite_SDFFont *font = agentite_sdf_font_generate(nullptr, "font.ttf", nullptr);
        REQUIRE(font == nullptr);
    }

    SECTION("agentite_sdf_font_destroy with NULL") {
        // Should not crash
        agentite_sdf_font_destroy(nullptr, nullptr);
    }

    SECTION("agentite_sdf_font_get_type with NULL") {
        Agentite_SDFFontType type = agentite_sdf_font_get_type(nullptr);
        // Should return a default value
        REQUIRE(static_cast<int>(type) >= 0);
    }

    SECTION("agentite_sdf_font_get_size with NULL") {
        float size = agentite_sdf_font_get_size(nullptr);
        REQUIRE(size == 0.0f);
    }

    SECTION("agentite_sdf_font_get_line_height with NULL") {
        float height = agentite_sdf_font_get_line_height(nullptr);
        REQUIRE(height == 0.0f);
    }

    SECTION("agentite_sdf_font_get_ascent with NULL") {
        float ascent = agentite_sdf_font_get_ascent(nullptr);
        REQUIRE(ascent == 0.0f);
    }

    SECTION("agentite_sdf_font_get_descent with NULL") {
        float descent = agentite_sdf_font_get_descent(nullptr);
        REQUIRE(descent == 0.0f);
    }
}

/* ============================================================================
 * SDF Text Drawing NULL Safety Tests
 * ============================================================================ */

TEST_CASE("SDF text drawing NULL safety", "[text][sdf][draw][null]") {
    SECTION("agentite_sdf_text_draw with NULL") {
        // Should not crash
        agentite_sdf_text_draw(nullptr, nullptr, "Hello", 100.0f, 200.0f, 1.0f);
    }

    SECTION("agentite_sdf_text_draw_colored with NULL") {
        // Should not crash
        agentite_sdf_text_draw_colored(nullptr, nullptr, "Hello", 100.0f, 200.0f, 1.0f,
                                      1.0f, 0.0f, 0.0f, 1.0f);
    }

    SECTION("agentite_sdf_text_draw_ex with NULL") {
        // Should not crash
        agentite_sdf_text_draw_ex(nullptr, nullptr, "Hello", 100.0f, 200.0f, 1.0f,
                                 1.0f, 1.0f, 1.0f, 1.0f, AGENTITE_TEXT_ALIGN_LEFT);
    }

    SECTION("agentite_sdf_text_printf with NULL") {
        // Should not crash
        agentite_sdf_text_printf(nullptr, nullptr, 100.0f, 200.0f, 1.0f, "Score: %d", 42);
    }

    SECTION("agentite_sdf_text_printf_colored with NULL") {
        // Should not crash
        agentite_sdf_text_printf_colored(nullptr, nullptr, 100.0f, 200.0f, 1.0f,
                                        1.0f, 0.0f, 0.0f, 1.0f, "Score: %d", 42);
    }
}

/* ============================================================================
 * SDF Text Effects NULL Safety Tests
 * ============================================================================ */

TEST_CASE("SDF text effects NULL safety", "[text][sdf][effects][null]") {
    SECTION("agentite_sdf_text_set_effects with NULL renderer") {
        Agentite_TextEffects effects = {};
        // Should not crash
        agentite_sdf_text_set_effects(nullptr, &effects);
    }

    SECTION("agentite_sdf_text_set_effects with NULL effects") {
        // Should not crash
        agentite_sdf_text_set_effects(nullptr, nullptr);
    }

    SECTION("agentite_sdf_text_clear_effects with NULL") {
        // Should not crash
        agentite_sdf_text_clear_effects(nullptr);
    }

    SECTION("agentite_sdf_text_set_outline with NULL") {
        // Should not crash
        agentite_sdf_text_set_outline(nullptr, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    SECTION("agentite_sdf_text_set_shadow with NULL") {
        // Should not crash
        agentite_sdf_text_set_shadow(nullptr, 2.0f, 2.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.7f);
    }

    SECTION("agentite_sdf_text_set_glow with NULL") {
        // Should not crash
        agentite_sdf_text_set_glow(nullptr, 0.2f, 1.0f, 1.0f, 0.0f, 1.0f);
    }

    SECTION("agentite_sdf_text_set_weight with NULL") {
        // Should not crash
        agentite_sdf_text_set_weight(nullptr, 0.2f);
    }
}

/* ============================================================================
 * SDF Text Measurement NULL Safety Tests
 * ============================================================================ */

TEST_CASE("SDF text measurement NULL safety", "[text][sdf][measure][null]") {
    SECTION("agentite_sdf_text_measure with NULL font") {
        float width = agentite_sdf_text_measure(nullptr, "Hello", 1.0f);
        REQUIRE(width == 0.0f);
    }

    SECTION("agentite_sdf_text_measure with NULL text") {
        float width = agentite_sdf_text_measure(nullptr, nullptr, 1.0f);
        REQUIRE(width == 0.0f);
    }

    SECTION("agentite_sdf_text_measure_bounds with NULL font") {
        float w = -1.0f, h = -1.0f;
        agentite_sdf_text_measure_bounds(nullptr, "Hello", 1.0f, &w, &h);
        // Should not crash, values may be 0 or unchanged
    }

    SECTION("agentite_sdf_text_measure_bounds with NULL text") {
        float w = -1.0f, h = -1.0f;
        agentite_sdf_text_measure_bounds(nullptr, nullptr, 1.0f, &w, &h);
        // Should not crash
    }

    SECTION("agentite_sdf_text_measure_bounds with NULL outputs") {
        agentite_sdf_text_measure_bounds(nullptr, "Hello", 1.0f, nullptr, nullptr);
        // Should not crash
    }
}
