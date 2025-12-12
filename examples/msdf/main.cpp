/**
 * Agentite Engine - MSDF (Multi-channel Signed Distance Field) Demo
 *
 * This demo showcases the MSDF text rendering system which provides:
 * - Sharp text at any scale (zoom in/out without blur)
 * - Text effects: outlines, shadows, and glows
 * - Weight adjustment (thin to bold)
 * - Runtime MSDF generation from TTF files
 * - Pre-generated MSDF atlas loading
 *
 * Controls:
 * - Mouse wheel: Zoom in/out
 * - Left-click drag: Pan view
 * - 1-5: Switch demo pages
 * - Space: Toggle effects animation
 * - R: Reset zoom and pan
 * - Escape: Quit
 */

#include "agentite/agentite.h"
#include "agentite/text.h"
#include "agentite/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Demo state */
typedef struct {
    float time;
    int current_page;
    bool animate_effects;
    float zoom;
    float target_zoom;
    float pan_x;         /* Pan offset X */
    float pan_y;         /* Pan offset Y */
    float target_pan_x;  /* Target pan X for smooth interpolation */
    float target_pan_y;  /* Target pan Y for smooth interpolation */
    Uint64 gen_time_ms;  /* Runtime font generation time */
} DemoState;

/* Forward declarations */
static void render_page_overview(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                  Agentite_Font *bitmap_font, DemoState *state, int width, int height);
static void render_page_scaling(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                 Agentite_Font *bitmap_font, DemoState *state, int width, int height);
static void render_page_effects(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                 DemoState *state, int width, int height);
static void render_page_colors(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                DemoState *state, int width, int height);
static void render_page_runtime(Agentite_TextRenderer *text, Agentite_SDFFont *runtime_font,
                                 DemoState *state, int width, int height);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure engine */
    Agentite_Config config = {
        .window_title = "Agentite - MSDF Text Rendering Demo",
        .window_width = 1280,
        .window_height = 720,
        .fullscreen = false,
        .vsync = true
    };

    /* Initialize engine */
    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize Agentite Engine\n");
        return 1;
    }

    /* Initialize text renderer */
    Agentite_TextRenderer *text = agentite_text_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );
    if (!text) {
        fprintf(stderr, "Failed to initialize text renderer\n");
        agentite_shutdown(engine);
        return 1;
    }

    /* Load pre-generated MSDF font */
    Agentite_SDFFont *msdf_font = agentite_sdf_font_load(text,
        "assets/fonts/Roboto-Regular-msdf.png",
        "assets/fonts/Roboto-Regular-msdf.json");

    if (!msdf_font) {
        fprintf(stderr, "Failed to load MSDF font atlas\n");
        fprintf(stderr, "Make sure assets/fonts/Roboto-Regular-msdf.png and .json exist\n");
        agentite_text_shutdown(text);
        agentite_shutdown(engine);
        return 1;
    }

    SDL_Log("MSDF font loaded (type: %s)",
            agentite_sdf_font_get_type(msdf_font) == AGENTITE_SDF_TYPE_MSDF ? "MSDF" : "SDF");

    /* Load bitmap font for comparison */
    Agentite_Font *bitmap_font = agentite_font_load(text, "assets/fonts/Roboto-Regular.ttf", 24.0f);
    if (!bitmap_font) {
        SDL_Log("Warning: Could not load bitmap font for comparison");
    }

    /* Generate MSDF font at runtime (demonstrates runtime generation)
     * Now that distance calculation bugs are fixed, we can use reasonable settings
     * similar to pre-generated atlases (which use 320x320 @ 48px scale) */
    Agentite_SDFFontGenConfig gen_config = AGENTITE_SDF_FONT_GEN_CONFIG_DEFAULT;
    gen_config.atlas_width = 512;
    gen_config.atlas_height = 512;
    gen_config.glyph_scale = 48.0f;    /* Match pre-generated quality */
    gen_config.pixel_range = 4.0f;     /* Standard SDF range */
    gen_config.generate_msdf = true;

    SDL_Log("Generating MSDF font at runtime...");
    Uint64 start_time = SDL_GetTicks();

    Agentite_SDFFont *runtime_font = agentite_sdf_font_generate(text,
        "assets/fonts/Roboto-Regular.ttf", &gen_config);

    Uint64 gen_time = SDL_GetTicks() - start_time;

    if (runtime_font) {
        SDL_Log("Runtime MSDF font generated in %llu ms", (unsigned long long)gen_time);
        /* Wait for GPU to finish uploading the texture */
        SDL_GPUDevice *gpu = agentite_get_gpu_device(engine);
        if (gpu) {
            SDL_WaitForGPUIdle(gpu);
        }
    } else {
        SDL_Log("Warning: Runtime MSDF generation failed (using pre-generated for all demos)");
        runtime_font = msdf_font;  /* Fallback */
    }

    /* Initialize input system */
    Agentite_Input *input = agentite_input_init();
    if (!input) {
        fprintf(stderr, "Failed to initialize input system\n");
        agentite_sdf_font_destroy(text, msdf_font);
        if (runtime_font != msdf_font) agentite_sdf_font_destroy(text, runtime_font);
        if (bitmap_font) agentite_font_destroy(text, bitmap_font);
        agentite_text_shutdown(text);
        agentite_shutdown(engine);
        return 1;
    }

    /* Register input actions */
    int action_quit = agentite_input_register_action(input, "quit");
    int action_page1 = agentite_input_register_action(input, "page1");
    int action_page2 = agentite_input_register_action(input, "page2");
    int action_page3 = agentite_input_register_action(input, "page3");
    int action_page4 = agentite_input_register_action(input, "page4");
    int action_page5 = agentite_input_register_action(input, "page5");
    int action_toggle_anim = agentite_input_register_action(input, "toggle_anim");
    int action_reset = agentite_input_register_action(input, "reset");

    agentite_input_bind_key(input, action_quit, SDL_SCANCODE_ESCAPE);
    agentite_input_bind_key(input, action_page1, SDL_SCANCODE_1);
    agentite_input_bind_key(input, action_page2, SDL_SCANCODE_2);
    agentite_input_bind_key(input, action_page3, SDL_SCANCODE_3);
    agentite_input_bind_key(input, action_page4, SDL_SCANCODE_4);
    agentite_input_bind_key(input, action_page5, SDL_SCANCODE_5);
    agentite_input_bind_key(input, action_toggle_anim, SDL_SCANCODE_SPACE);
    agentite_input_bind_key(input, action_reset, SDL_SCANCODE_R);

    /* Demo state */
    DemoState state = {
        .time = 0.0f,
        .current_page = 1,
        .animate_effects = true,
        .zoom = 1.0f,
        .target_zoom = 1.0f,
        .pan_x = 0.0f,
        .pan_y = 0.0f,
        .target_pan_x = 0.0f,
        .target_pan_y = 0.0f,
        .gen_time_ms = gen_time
    };

    /* Main loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        agentite_input_begin_frame(input);

        /* Process events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }

        agentite_input_update(input);

        float dt = agentite_get_delta_time(engine);
        state.time += dt;

        /* Handle input */
        if (agentite_input_action_just_pressed(input, action_quit)) {
            agentite_quit(engine);
        }
        if (agentite_input_action_just_pressed(input, action_page1)) state.current_page = 1;
        if (agentite_input_action_just_pressed(input, action_page2)) state.current_page = 2;
        if (agentite_input_action_just_pressed(input, action_page3)) state.current_page = 3;
        if (agentite_input_action_just_pressed(input, action_page4)) state.current_page = 4;
        if (agentite_input_action_just_pressed(input, action_page5)) state.current_page = 5;
        if (agentite_input_action_just_pressed(input, action_toggle_anim)) {
            state.animate_effects = !state.animate_effects;
        }
        if (agentite_input_action_just_pressed(input, action_reset)) {
            state.target_zoom = 1.0f;
            state.zoom = 1.0f;
            state.pan_x = 0.0f;
            state.pan_y = 0.0f;
            state.target_pan_x = 0.0f;
            state.target_pan_y = 0.0f;
        }

        /* Mouse wheel zoom - zoom relative to mouse position */
        float scroll_x, scroll_y;
        agentite_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y != 0) {
            float old_target_zoom = state.target_zoom;
            if (scroll_y > 0) state.target_zoom *= 1.15f;
            if (scroll_y < 0) state.target_zoom /= 1.15f;
            state.target_zoom = fmaxf(0.25f, fminf(8.0f, state.target_zoom));

            /* Get mouse position */
            float mouse_x, mouse_y;
            agentite_input_get_mouse_position(input, &mouse_x, &mouse_y);

            /* Calculate content point under mouse at current target state */
            /* content = (screen - pan) / zoom */
            float content_x = (mouse_x - state.target_pan_x) / old_target_zoom;
            float content_y = (mouse_y - state.target_pan_y) / old_target_zoom;

            /* Calculate new target pan to keep that content point under mouse */
            /* new_pan = screen - content * new_zoom */
            state.target_pan_x = mouse_x - content_x * state.target_zoom;
            state.target_pan_y = mouse_y - content_y * state.target_zoom;
        }

        /* Smooth interpolation of zoom and pan together */
        float lerp_speed = 10.0f * dt;
        state.zoom += (state.target_zoom - state.zoom) * lerp_speed;
        state.pan_x += (state.target_pan_x - state.pan_x) * lerp_speed;
        state.pan_y += (state.target_pan_y - state.pan_y) * lerp_speed;

        /* Mouse drag panning (button index 0 = left button, since internal array is 0-indexed) */
        if (agentite_input_mouse_button(input, 0)) {
            float dx, dy;
            agentite_input_get_mouse_delta(input, &dx, &dy);
            /* Apply drag to both current and target pan for immediate response */
            state.pan_x += dx;
            state.pan_y += dy;
            state.target_pan_x += dx;
            state.target_pan_y += dy;
        }

        /* Acquire command buffer */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Build text batches */
            agentite_text_begin(text);

            /* Clear effects for header */
            agentite_sdf_text_clear_effects(text);

            /* Draw page title and controls */
            const char *page_names[] = {
                "1: Overview",
                "2: Scaling",
                "3: Effects",
                "4: Colors",
                "5: Runtime Gen"
            };

            agentite_sdf_text_draw_colored(text, msdf_font,
                "MSDF Text Rendering Demo", 20.0f, 35.0f, 0.9f,
                1.0f, 1.0f, 1.0f, 1.0f);

            /* Page selector */
            for (int i = 0; i < 5; i++) {
                float x = 20.0f + i * 150.0f;
                float alpha = (state.current_page == i + 1) ? 1.0f : 0.5f;
                agentite_sdf_text_draw_colored(text, msdf_font,
                    page_names[i], x, 75.0f, 0.5f,
                    0.7f, 0.9f, 1.0f, alpha);
            }

            /* Zoom indicator */
            char zoom_text[32];
            snprintf(zoom_text, sizeof(zoom_text), "Zoom: %.1fx", state.zoom);
            agentite_sdf_text_draw_colored(text, msdf_font,
                zoom_text, (float)(config.window_width - 120), 35.0f, 0.5f,
                0.8f, 0.8f, 0.8f, 1.0f);

            /* Render current page */
            switch (state.current_page) {
                case 1:
                    render_page_overview(text, msdf_font, bitmap_font, &state,
                                        config.window_width, config.window_height);
                    break;
                case 2:
                    render_page_scaling(text, msdf_font, bitmap_font, &state,
                                       config.window_width, config.window_height);
                    break;
                case 3:
                    render_page_effects(text, msdf_font, &state,
                                       config.window_width, config.window_height);
                    break;
                case 4:
                    render_page_colors(text, msdf_font, &state,
                                      config.window_width, config.window_height);
                    break;
                case 5:
                    /* Need batch break when switching to different SDF font */
                    agentite_text_end(text);
                    agentite_text_begin(text);
                    render_page_runtime(text, runtime_font, &state,
                                       config.window_width, config.window_height);
                    break;
            }

            /* Controls hint - render last so it's always on top at bottom of screen */
            agentite_text_end(text);
            agentite_text_begin(text);
            agentite_sdf_text_clear_effects(text);
            agentite_sdf_text_draw_colored(text, msdf_font,
                "Scroll: Zoom | Drag: Pan | Space: Toggle Animation | R: Reset | ESC: Quit",
                20.0f, (float)(config.window_height - 35), 0.5f,
                0.5f, 0.5f, 0.5f, 1.0f);

            agentite_text_end(text);
            agentite_text_upload(text, cmd);

            /* Begin render pass with dark background */
            if (agentite_begin_render_pass(engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                agentite_text_render(text, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_input_shutdown(input);
    if (runtime_font != msdf_font) agentite_sdf_font_destroy(text, runtime_font);
    agentite_sdf_font_destroy(text, msdf_font);
    if (bitmap_font) agentite_font_destroy(text, bitmap_font);
    agentite_text_shutdown(text);
    agentite_shutdown(engine);

    return 0;
}

/* Page 1: Overview - MSDF vs Bitmap comparison */
static void render_page_overview(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                  Agentite_Font *bitmap_font, DemoState *state, int width, int height) {
    (void)width;
    (void)height;

    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float margin = 40.0f;

    agentite_sdf_text_clear_effects(text);

    /* Title */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "What is MSDF?", margin * z + px, 160.0f * z + py, 1.2f * z,
        1.0f, 0.9f, 0.4f, 1.0f);

    /* Explanation */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Multi-channel Signed Distance Field - sharp text at any scale",
        margin * z + px, 200.0f * z + py, 0.55f * z,
        0.7f, 0.7f, 0.7f, 1.0f);

    /* MSDF sample label */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "MSDF Text (scales perfectly):", margin * z + px, 260.0f * z + py, 0.6f * z,
        0.4f, 1.0f, 0.6f, 1.0f);

    /* MSDF sample text */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "The quick brown fox jumps over the lazy dog!",
        (margin + 20.0f) * z + px, 320.0f * z + py, 1.1f * z,
        1.0f, 1.0f, 1.0f, 1.0f);

    /* Bitmap comparison */
    if (bitmap_font) {
        agentite_sdf_text_draw_colored(text, msdf_font,
            "Bitmap Text (blurs when scaled):", margin * z + px, 380.0f * z + py, 0.6f * z,
            1.0f, 0.6f, 0.4f, 1.0f);

        agentite_text_end(text);
        agentite_text_begin(text);

        agentite_text_draw_scaled(text, bitmap_font,
            "The quick brown fox jumps over the lazy dog!",
            (margin + 20.0f) * z + px, 430.0f * z + py, 1.8f * z);

        agentite_text_end(text);
        agentite_text_begin(text);
    }

    /* Key Features heading */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Key Features:", margin * z + px, 500.0f * z + py, 0.7f * z,
        1.0f, 0.9f, 0.4f, 1.0f);

    /* Feature list */
    const char *features[] = {
        "- Sharp text at any zoom level",
        "- GPU-accelerated rendering",
        "- Outlines, shadows & glows",
        "- Weight adjustment (thin to bold)",
        "- Runtime TTF font loading",
        "- Low memory footprint"
    };

    float feature_y = 535.0f;
    for (int i = 0; i < 6; i++) {
        float pulse = state->animate_effects ? 0.8f + 0.2f * sinf(state->time * 2.0f + i * 0.5f) : 1.0f;
        agentite_sdf_text_draw_colored(text, msdf_font,
            features[i], (margin + 20.0f) * z + px, feature_y * z + py, 0.5f * z,
            0.6f * pulse, 0.8f * pulse, 1.0f * pulse, 1.0f);
        feature_y += 20.0f;
    }
}

/* Page 2: Scaling - Demonstrate scale independence */
static void render_page_scaling(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                 Agentite_Font *bitmap_font, DemoState *state, int width, int height) {
    (void)width;
    (void)height;
    (void)bitmap_font;

    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float margin = 40.0f;

    agentite_sdf_text_clear_effects(text);

    /* Title */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Scale Independence", margin * z + px, 160.0f * z + py, 1.2f * z,
        1.0f, 0.9f, 0.4f, 1.0f);

    /* Various scales - fit within window */
    float scales[] = { 0.4f, 0.6f, 0.8f, 1.0f, 1.3f };
    const char *scale_labels[] = { "0.4x", "0.6x", "0.8x", "1.0x", "1.3x" };
    float y_positions[] = { 220.0f, 300.0f, 390.0f, 490.0f, 600.0f };

    for (int i = 0; i < 5; i++) {
        float scale = scales[i] * z;
        char label[64];
        snprintf(label, sizeof(label), "%s: Sharp at any scale!", scale_labels[i]);

        /* Animated rainbow colors */
        float hue = (float)i / 5.0f + (state->animate_effects ? state->time * 0.1f : 0.0f);
        float r = 0.5f + 0.5f * sinf(hue * 6.28f);
        float g = 0.5f + 0.5f * sinf((hue + 0.33f) * 6.28f);
        float b = 0.5f + 0.5f * sinf((hue + 0.66f) * 6.28f);

        agentite_sdf_text_draw_colored(text, msdf_font,
            label, margin * z + px, y_positions[i] * z + py, scale,
            r, g, b, 1.0f);
    }
}

/* Page 3: Effects - Outlines, shadows, glows */
static void render_page_effects(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                 DemoState *state, int width, int height) {
    (void)width;
    (void)height;

    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float margin = 40.0f;

    agentite_sdf_text_clear_effects(text);

    /* Title */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Text Effects", margin * z + px, 160.0f * z + py, 1.2f * z,
        1.0f, 0.9f, 0.4f, 1.0f);

    /* No effects (baseline) */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "No Effects", margin * z + px, 220.0f * z + py, 0.9f * z,
        1.0f, 1.0f, 1.0f, 1.0f);

    /* Blue outline */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float outline_width = state->animate_effects ? 0.15f + 0.05f * sinf(state->time * 3.0f) : 0.18f;
    agentite_sdf_text_set_outline(text, outline_width, 0.2f, 0.6f, 1.0f, 1.0f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Blue Outline", margin * z + px, 280.0f * z + py, 0.9f * z,
        1.0f, 1.0f, 1.0f, 1.0f);

    /* Shadow effect */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float shadow_offset = state->animate_effects ? 4.0f + 2.0f * sinf(state->time * 2.0f) : 5.0f;
    agentite_sdf_text_set_shadow(text, shadow_offset, shadow_offset, 0.5f, 0.5f, 0.5f, 0.6f, 0.8f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Shadow Effect", margin * z + px, 340.0f * z + py, 0.9f * z,
        1.0f, 0.9f, 0.7f, 1.0f);

    /* Cyan glow */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float glow_width = state->animate_effects ? 0.2f + 0.1f * sinf(state->time * 4.0f) : 0.25f;
    agentite_sdf_text_set_glow(text, glow_width, 0.2f, 0.8f, 1.0f, 1.0f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Cyan Glow", margin * z + px, 400.0f * z + py, 0.9f * z,
        0.9f, 0.95f, 1.0f, 1.0f);

    /* Combined: Outline + Glow */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_outline(text, 0.12f, 0.2f, 0.1f, 0.0f, 1.0f);
    float combined_glow = state->animate_effects ? 0.3f + 0.12f * sinf(state->time * 2.5f) : 0.38f;
    agentite_sdf_text_set_glow(text, combined_glow, 0.4f, 1.0f, 0.2f, 1.0f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Outline + Glow", margin * z + px, 460.0f * z + py, 0.9f * z,
        1.0f, 1.0f, 1.0f, 1.0f);

    /* Weight adjustment section */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Weight Adjustment:", margin * z + px, 530.0f * z + py, 0.7f * z,
        0.7f, 0.7f, 0.7f, 1.0f);

    float weights[] = { -0.3f, -0.15f, 0.0f, 0.15f, 0.3f };
    const char *weight_labels[] = { "Thin", "Light", "Normal", "Bold", "Heavy" };
    float weight_x[] = { 40.0f, 200.0f, 380.0f, 560.0f, 740.0f };

    for (int i = 0; i < 5; i++) {
        agentite_text_end(text);
        agentite_text_begin(text);
        agentite_sdf_text_clear_effects(text);
        agentite_sdf_text_set_weight(text, weights[i]);
        agentite_sdf_text_draw_colored(text, msdf_font,
            weight_labels[i], weight_x[i] * z + px, 590.0f * z + py, 0.8f * z,
            0.9f, 0.9f, 0.9f, 1.0f);
    }

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
}

/* Page 4: Colors - Vibrant colored text */
static void render_page_colors(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                DemoState *state, int width, int height) {
    (void)width;
    (void)height;

    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float margin = 40.0f;

    agentite_sdf_text_clear_effects(text);

    /* Title */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Color Showcase", margin * z + px, 160.0f * z + py, 1.2f * z,
        1.0f, 0.9f, 0.4f, 1.0f);

    /* Rainbow text */
    const char *rainbow_words[] = { "Red", "Orange", "Yellow", "Green", "Blue", "Purple" };
    float rainbow_colors[][3] = {
        { 1.0f, 0.2f, 0.2f },
        { 1.0f, 0.6f, 0.2f },
        { 1.0f, 1.0f, 0.2f },
        { 0.2f, 1.0f, 0.2f },
        { 0.2f, 0.4f, 1.0f },
        { 0.8f, 0.2f, 1.0f }
    };

    float x = margin * z + px;
    for (int i = 0; i < 6; i++) {
        float pulse = state->animate_effects ? 0.7f + 0.3f * sinf(state->time * 3.0f + i * 1.0f) : 1.0f;
        agentite_sdf_text_draw_colored(text, msdf_font,
            rainbow_words[i], x, 220.0f * z + py, 0.6f * z,
            rainbow_colors[i][0] * pulse,
            rainbow_colors[i][1] * pulse,
            rainbow_colors[i][2] * pulse,
            1.0f);
        x += 100.0f * z;
    }

    /* Neon text */
    float neon_pulse = state->animate_effects ? 0.6f + 0.4f * sinf(state->time * 5.0f) : 1.0f;

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_glow(text, 0.3f, 0.0f, 1.0f, 0.5f, neon_pulse * 0.8f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "NEON", margin * z + px, 310.0f * z + py, 1.2f * z,
        0.0f, 1.0f * neon_pulse, 0.5f * neon_pulse, 1.0f);

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_glow(text, 0.3f, 1.0f, 0.0f, 0.5f, neon_pulse * 0.8f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "LIGHTS", 220.0f * z + px, 310.0f * z + py, 1.2f * z,
        1.0f * neon_pulse, 0.0f, 0.5f * neon_pulse, 1.0f);

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);

    /* Transparency demo */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Transparency:", margin * z + px, 400.0f * z + py, 0.6f * z,
        0.7f, 0.7f, 0.7f, 1.0f);

    float alphas[] = { 1.0f, 0.75f, 0.5f, 0.25f };
    x = (margin + 20.0f) * z + px;
    for (int i = 0; i < 4; i++) {
        char label[32];
        snprintf(label, sizeof(label), "%.0f%%", alphas[i] * 100);
        agentite_sdf_text_draw_colored(text, msdf_font,
            label, x, 450.0f * z + py, 0.6f * z,
            1.0f, 1.0f, 1.0f, alphas[i]);
        x += 80.0f * z;
    }

    /* Color cycling */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Color Cycling:", margin * z + px, 520.0f * z + py, 0.6f * z,
        0.7f, 0.7f, 0.7f, 1.0f);

    if (state->animate_effects) {
        float hue = fmodf(state->time * 0.5f, 1.0f);
        float r = 0.5f + 0.5f * sinf(hue * 6.28f);
        float g = 0.5f + 0.5f * sinf((hue + 0.33f) * 6.28f);
        float b = 0.5f + 0.5f * sinf((hue + 0.66f) * 6.28f);

        agentite_sdf_text_set_outline(text, 0.15f, 1.0f - r, 1.0f - g, 1.0f - b, 1.0f);
        agentite_sdf_text_draw_colored(text, msdf_font,
            "Smoothly Cycling Colors", (margin + 20.0f) * z + px, 580.0f * z + py, 0.8f * z,
            r, g, b, 1.0f);
    } else {
        agentite_sdf_text_draw_colored(text, msdf_font,
            "Press SPACE to animate", (margin + 20.0f) * z + px, 580.0f * z + py, 0.8f * z,
            0.5f, 0.5f, 0.5f, 1.0f);
    }

    agentite_sdf_text_clear_effects(text);
}

/* Page 5: Runtime Generation */
static void render_page_runtime(Agentite_TextRenderer *text, Agentite_SDFFont *runtime_font,
                                 DemoState *state, int width, int height) {
    (void)width;
    (void)height;

    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float margin = 40.0f;

    agentite_sdf_text_clear_effects(text);

    /* Title */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "Runtime MSDF Generation", margin * z + px, 160.0f * z + py, 1.2f * z,
        1.0f, 0.9f, 0.4f, 1.0f);

    /* Explanation */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "Generate MSDF fonts at runtime from TTF files - no external tools needed!",
        margin * z + px, 210.0f * z + py, 0.55f * z,
        0.7f, 0.7f, 0.7f, 1.0f);

    /* Runtime font demo text */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "This text uses a runtime-generated MSDF font!",
        margin * z + px, 270.0f * z + py, 0.8f * z,
        1.0f, 0.8f, 0.4f, 1.0f);

    /* Effects demos */
    float effect_time = state->animate_effects ? state->time : 0.0f;

    /* Outline */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_outline(text, 0.2f, 0.3f, 0.7f, 1.0f, 1.0f);
    agentite_sdf_text_draw_colored(text, runtime_font,
        "With Outline", margin * z + px, 340.0f * z + py, 0.9f * z,
        1.0f, 1.0f, 1.0f, 1.0f);

    /* Glow */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float glow = 0.25f + 0.1f * sinf(effect_time * 3.0f);
    agentite_sdf_text_set_glow(text, glow, 1.0f, 0.4f, 0.8f, 1.0f);
    agentite_sdf_text_draw_colored(text, runtime_font,
        "With Glow", margin * z + px, 400.0f * z + py, 0.9f * z,
        1.0f, 0.8f, 0.9f, 1.0f);

    /* Shadow */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_shadow(text, 5.0f, 5.0f, 0.5f, 0.5f, 0.5f, 0.6f, 0.9f);
    agentite_sdf_text_draw_colored(text, runtime_font,
        "With Shadow", margin * z + px, 460.0f * z + py, 0.9f * z,
        1.0f, 0.95f, 0.8f, 1.0f);

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);

    /* Config info */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "Generation Config:", margin * z + px, 530.0f * z + py, 0.7f * z,
        0.6f, 0.6f, 0.6f, 1.0f);

    char config_lines[4][64];
    snprintf(config_lines[0], sizeof(config_lines[0]), "Atlas: 512x512  |  Glyph scale: 48px");
    snprintf(config_lines[1], sizeof(config_lines[1]), "Pixel range: 4.0  |  Charset: ASCII");
    snprintf(config_lines[2], sizeof(config_lines[2]), "Generation time: %llu ms", (unsigned long long)state->gen_time_ms);

    float config_y = 570.0f;
    for (int i = 0; i < 3; i++) {
        float g_color = (i == 2) ? 1.0f : 0.7f;
        agentite_sdf_text_draw_colored(text, runtime_font,
            config_lines[i], (margin + 20.0f) * z + px, config_y * z + py, 0.5f * z,
            0.5f, g_color, 0.6f, 1.0f);
        config_y += 25.0f;
    }
}
