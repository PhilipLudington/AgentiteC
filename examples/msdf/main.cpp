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
                "MSDF Text Rendering Demo", 20.0f, 35.0f, 0.8f,
                1.0f, 1.0f, 1.0f, 1.0f);

            /* Page selector */
            for (int i = 0; i < 5; i++) {
                float x = 20.0f + i * 150.0f;
                float alpha = (state.current_page == i + 1) ? 1.0f : 0.5f;
                agentite_sdf_text_draw_colored(text, msdf_font,
                    page_names[i], x, 70.0f, 0.5f,
                    0.7f, 0.9f, 1.0f, alpha);
            }

            /* Controls hint */
            agentite_sdf_text_draw_colored(text, msdf_font,
                "Scroll: Zoom | Drag: Pan | Space: Toggle Animation | R: Reset | ESC: Quit",
                20.0f, (float)(config.window_height - 30), 0.4f,
                0.6f, 0.6f, 0.6f, 1.0f);

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

    /* Scale positions by zoom so text zooms relative to each other */
    float z = state->zoom;
    float px = state->pan_x;  /* Pan offset */
    float py = state->pan_y;
    const float content_top = 130.0f * z + py;
    const float left_margin = 40.0f * z + px;      /* Headers/labels at left edge */
    const float indent_x = 80.0f * z + px;         /* Sample text/content indented */

    float y = content_top;

    agentite_sdf_text_clear_effects(text);

    /* Title */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "What is MSDF?", left_margin, y, 1.0f * z,
        1.0f, 0.9f, 0.4f, 1.0f);
    y += 60.0f * z;

    /* Explanation - indented under title */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "MSDF (Multi-channel Signed Distance Field) stores distance-to-edge", indent_x, y, 0.5f * z,
        0.9f, 0.9f, 0.9f, 1.0f);
    y += 30.0f * z;

    agentite_sdf_text_draw_colored(text, msdf_font,
        "information in RGB channels, enabling sharp text at any scale.", indent_x, y, 0.5f * z,
        0.9f, 0.9f, 0.9f, 1.0f);
    y += 50.0f * z;

    /* MSDF sample section label */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "MSDF Text (scales perfectly):", left_margin, y, 0.6f * z,
        0.4f, 1.0f, 0.6f, 1.0f);
    y += 60.0f * z;

    /* Large MSDF sample text - indented */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "The quick brown fox jumps!", indent_x, y, 1.2f * z,
        1.0f, 1.0f, 1.0f, 1.0f);
    y += 70.0f * z;

    /* Bitmap comparison */
    if (bitmap_font) {
        agentite_sdf_text_draw_colored(text, msdf_font,
            "Bitmap Text (blurs when scaled):", left_margin, y, 0.6f * z,
            1.0f, 0.6f, 0.4f, 1.0f);
        y += 60.0f * z;

        /* End MSDF batch, start bitmap batch */
        agentite_text_end(text);
        agentite_text_begin(text);

        /* Bitmap sample text - indented */
        agentite_text_draw_scaled(text, bitmap_font,
            "The quick brown fox jumps!", indent_x, y, 2.0f * z);

        /* End bitmap batch, start new MSDF batch */
        agentite_text_end(text);
        agentite_text_begin(text);

        y += 70.0f * z;
    }

    /* Features list - render all, let GPU clip what goes off screen */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "MSDF Features:", left_margin, y, 0.7f * z,
        1.0f, 0.9f, 0.4f, 1.0f);
    y += 45.0f * z;

    const char *features[] = {
        "Sharp text at any zoom level",
        "GPU-accelerated rendering",
        "Outline, shadow, and glow effects",
        "Weight adjustment (thin to bold)",
        "Runtime generation from TTF files",
        "Small texture memory footprint"
    };

    for (int i = 0; i < 6; i++) {
        float pulse = state->animate_effects ? 0.8f + 0.2f * sinf(state->time * 2.0f + i * 0.5f) : 1.0f;
        agentite_sdf_text_draw_colored(text, msdf_font,
            features[i], indent_x, y, 0.5f * z,
            0.7f * pulse, 0.9f * pulse, 1.0f * pulse, 1.0f);
        y += 30.0f * z;
    }
}

/* Page 2: Scaling - Demonstrate scale independence */
static void render_page_scaling(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                 Agentite_Font *bitmap_font, DemoState *state, int width, int height) {
    (void)width;
    (void)height;

    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float y = 120.0f * z + py;

    agentite_sdf_text_clear_effects(text);

    agentite_sdf_text_draw_colored(text, msdf_font,
        "Scale Independence", 40.0f * z + px, y, 1.0f * z,
        1.0f, 0.9f, 0.4f, 1.0f);
    y += 60.0f * z;

    /* Various scales */
    float scales[] = { 0.3f, 0.5f, 0.8f, 1.0f, 1.5f, 2.0f, 3.0f };
    const char *scale_labels[] = { "0.3x", "0.5x", "0.8x", "1.0x", "1.5x", "2.0x", "3.0x" };

    for (int i = 0; i < 7; i++) {
        float scale = scales[i] * z;
        char label[64];
        snprintf(label, sizeof(label), "%s: MSDF Sharp Text", scale_labels[i]);

        /* Animated color */
        float hue = (float)i / 7.0f + (state->animate_effects ? state->time * 0.1f : 0.0f);
        float r = 0.5f + 0.5f * sinf(hue * 6.28f);
        float g = 0.5f + 0.5f * sinf((hue + 0.33f) * 6.28f);
        float b = 0.5f + 0.5f * sinf((hue + 0.66f) * 6.28f);

        agentite_sdf_text_draw_colored(text, msdf_font,
            label, 40.0f * z + px, y, scale,
            r, g, b, 1.0f);

        /* Spacing must account for the NEXT line's height since scales increase.
         * Look ahead to get the next scale, or use current scale for last line. */
        float next_scale = (i < 6) ? scales[i + 1] * z : scale;
        y += 48.0f * next_scale + 10.0f * z;
    }

    /* Helper text - continue from accumulated y position with padding */
    y += 30.0f * z;

    agentite_sdf_text_draw_colored(text, msdf_font,
        "Use mouse wheel to zoom - text stays sharp!", 40.0f * z + px, y, 0.6f * z,
        0.6f, 0.8f, 1.0f, 1.0f);

    if (bitmap_font) {
        y += 40.0f * z;
        agentite_sdf_text_draw_colored(text, msdf_font,
            "(Bitmap fonts would blur at non-native sizes)", 40.0f * z + px, y, 0.5f * z,
            0.5f, 0.5f, 0.5f, 1.0f);
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
    float y = 120.0f * z + py;

    /* NOTE: Effects are captured per-batch, so we need to end/begin batches
     * when changing effects to ensure each text gets its own effect settings */

    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Text Effects", 40.0f * z + px, y, 1.0f * z,
        1.0f, 0.9f, 0.4f, 1.0f);
    y += 70.0f * z;

    /* No effects (baseline) - new batch */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "No Effects (baseline)", 60.0f * z + px, y, 0.8f * z,
        1.0f, 1.0f, 1.0f, 1.0f);
    y += 60.0f * z;

    /* Outline effect - new batch */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float outline_width = state->animate_effects ? 0.15f + 0.05f * sinf(state->time * 3.0f) : 0.18f;
    agentite_sdf_text_set_outline(text, outline_width, 0.2f, 0.6f, 1.0f, 1.0f);  /* Blue outline */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Outline Effect", 60.0f * z + px, y, 0.8f * z,
        1.0f, 1.0f, 1.0f, 1.0f);
    y += 70.0f * z;

    /* Different outline color - new batch */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_outline(text, 0.2f, 1.0f, 0.2f, 0.2f, 1.0f);  /* Red outline */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Red Outline", 60.0f * z + px, y, 0.8f * z,
        1.0f, 1.0f, 0.8f, 1.0f);
    y += 70.0f * z;

    /* Shadow effect - use light shadow color so it's visible on dark background */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float shadow_offset = state->animate_effects ? 4.0f + 2.0f * sinf(state->time * 2.0f) : 5.0f;
    agentite_sdf_text_set_shadow(text, shadow_offset, shadow_offset, 0.4f, 0.5f, 0.5f, 0.6f, 0.8f);  /* Light purple shadow */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Shadow Effect", 60.0f * z + px, y, 0.8f * z,
        1.0f, 0.9f, 0.7f, 1.0f);
    y += 70.0f * z;

    /* Glow effect - new batch */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float glow_width = state->animate_effects ? 0.15f + 0.08f * sinf(state->time * 4.0f) : 0.2f;
    agentite_sdf_text_set_glow(text, glow_width, 0.2f, 0.8f, 1.0f, 1.0f);  /* Cyan glow */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Glow Effect", 60.0f * z + px, y, 0.8f * z,
        0.9f, 0.95f, 1.0f, 1.0f);
    y += 70.0f * z;

    /* Combined effects - new batch
     * Glow must be wider than outline to be visible (glow renders behind outline) */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_outline(text, 0.1f, 0.2f, 0.1f, 0.0f, 1.0f);  /* Dark green outline */
    float combined_glow = state->animate_effects ? 0.28f + 0.1f * sinf(state->time * 2.5f) : 0.35f;
    agentite_sdf_text_set_glow(text, combined_glow, 0.4f, 1.0f, 0.2f, 1.0f);  /* Bright green glow */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Combined: Outline + Glow", 60.0f * z + px, y, 0.8f * z,
        1.0f, 1.0f, 1.0f, 1.0f);
    y += 70.0f * z;

    /* Weight adjustment label - new batch */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Weight Adjustment:", 60.0f * z + px, y, 0.6f * z,
        0.7f, 0.7f, 0.7f, 1.0f);
    y += 40.0f * z;

    float weights[] = { -0.3f, -0.15f, 0.0f, 0.15f, 0.3f };
    const char *weight_labels[] = { "Thin", "Light", "Normal", "Bold", "Heavy" };

    float x = 80.0f * z + px;
    for (int i = 0; i < 5; i++) {
        /* Each weight needs its own batch */
        agentite_text_end(text);
        agentite_text_begin(text);
        agentite_sdf_text_clear_effects(text);
        agentite_sdf_text_set_weight(text, weights[i]);
        agentite_sdf_text_draw_colored(text, msdf_font,
            weight_labels[i], x, y, 0.7f * z,
            0.9f, 0.9f, 0.9f, 1.0f);
        x += 150.0f * z;
    }

    /* Final batch cleanup */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
}

/* Page 4: Colors - Vibrant colored text */
static void render_page_colors(Agentite_TextRenderer *text, Agentite_SDFFont *msdf_font,
                                DemoState *state, int width, int height) {
    (void)width;
    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float y = 120.0f * z + py;

    agentite_sdf_text_clear_effects(text);

    agentite_sdf_text_draw_colored(text, msdf_font,
        "Color Showcase", 40.0f * z + px, y, 1.0f * z,
        1.0f, 0.9f, 0.4f, 1.0f);
    y += 70.0f * z;

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

    float x = 60.0f * z + px;
    for (int i = 0; i < 6; i++) {
        float pulse = state->animate_effects ? 0.7f + 0.3f * sinf(state->time * 3.0f + i * 1.0f) : 1.0f;
        agentite_sdf_text_draw_colored(text, msdf_font,
            rainbow_words[i], x, y, 0.7f * z,
            rainbow_colors[i][0] * pulse,
            rainbow_colors[i][1] * pulse,
            rainbow_colors[i][2] * pulse,
            1.0f);
        x += 115.0f * z;
    }
    y += 60.0f * z;

    /* Animated gradient effect (simulated with multiple draws) */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Animated Colors:", 60.0f * z + px, y, 0.6f * z,
        0.7f, 0.7f, 0.7f, 1.0f);
    y += 70.0f * z;

    /* Pulsing neon text - need separate batches for different glow effects */
    float neon_pulse = state->animate_effects ? 0.6f + 0.4f * sinf(state->time * 5.0f) : 1.0f;

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_glow(text, 0.3f, 0.0f, 1.0f, 0.5f, neon_pulse * 0.8f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "NEON", 60.0f * z + px, y, 1.5f * z,
        0.0f, 1.0f * neon_pulse, 0.5f * neon_pulse, 1.0f);

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_glow(text, 0.3f, 1.0f, 0.0f, 0.5f, neon_pulse * 0.8f);
    agentite_sdf_text_draw_colored(text, msdf_font,
        "LIGHTS", 280.0f * z + px, y, 1.5f * z,
        1.0f * neon_pulse, 0.0f, 0.5f * neon_pulse, 1.0f);
    y += 100.0f * z;

    agentite_text_end(text);
    agentite_text_begin(text);

    agentite_sdf_text_clear_effects(text);

    /* Transparency demo */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Transparency:", 60.0f * z + px, y, 0.6f * z,
        0.7f, 0.7f, 0.7f, 1.0f);
    y += 40.0f * z;

    float alphas[] = { 1.0f, 0.75f, 0.5f, 0.25f };
    x = 80.0f * z + px;
    for (int i = 0; i < 4; i++) {
        char label[32];
        snprintf(label, sizeof(label), "%.0f%%", alphas[i] * 100);
        agentite_sdf_text_draw_colored(text, msdf_font,
            label, x, y, 0.7f * z,
            1.0f, 1.0f, 1.0f, alphas[i]);
        x += 90.0f * z;
    }
    y += 50.0f * z;

    /* Color cycling demonstration */
    agentite_sdf_text_draw_colored(text, msdf_font,
        "Color Cycling:", 60.0f * z + px, y, 0.6f * z,
        0.7f, 0.7f, 0.7f, 1.0f);
    y += 50.0f * z;

    if (state->animate_effects) {
        float hue = fmodf(state->time * 0.5f, 1.0f);
        float r = 0.5f + 0.5f * sinf(hue * 6.28f);
        float g = 0.5f + 0.5f * sinf((hue + 0.33f) * 6.28f);
        float b = 0.5f + 0.5f * sinf((hue + 0.66f) * 6.28f);

        agentite_sdf_text_set_outline(text, 0.15f, 1.0f - r, 1.0f - g, 1.0f - b, 1.0f);
        agentite_sdf_text_draw_colored(text, msdf_font,
            "Smoothly Cycling Colors", 80.0f * z + px, y, 1.0f * z,
            r, g, b, 1.0f);
    } else {
        agentite_sdf_text_draw_colored(text, msdf_font,
            "Press SPACE to animate", 80.0f * z + px, y, 1.0f * z,
            0.5f, 0.5f, 0.5f, 1.0f);
    }

    agentite_sdf_text_clear_effects(text);

    /* Stats at bottom */
    y = (float)(height - 80) * z + py;
    char stats[128];
    snprintf(stats, sizeof(stats), "Current frame time: %.2f ms | Animation: %s",
             1000.0f / 60.0f, state->animate_effects ? "ON" : "OFF");
    agentite_sdf_text_draw_colored(text, msdf_font,
        stats, 40.0f * z + px, y, 0.4f * z,
        0.5f, 0.5f, 0.5f, 1.0f);
}

/* Page 5: Runtime Generation */
static void render_page_runtime(Agentite_TextRenderer *text, Agentite_SDFFont *runtime_font,
                                 DemoState *state, int width, int height) {
    (void)width;
    (void)height;

    float z = state->zoom;
    float px = state->pan_x;
    float py = state->pan_y;
    float y = 120.0f * z + py;

    agentite_sdf_text_clear_effects(text);

    agentite_sdf_text_draw_colored(text, runtime_font,
        "Runtime MSDF Generation", 40.0f * z + px, y, 1.0f * z,
        1.0f, 0.9f, 0.4f, 1.0f);
    y += 70.0f * z;

    /* Explanation */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "MSDF fonts can be generated at runtime from TTF files!", 40.0f * z + px, y, 0.55f * z,
        0.9f, 0.9f, 0.9f, 1.0f);
    y += 35.0f * z;

    agentite_sdf_text_draw_colored(text, runtime_font,
        "No need for external tools like msdf-atlas-gen.", 40.0f * z + px, y, 0.55f * z,
        0.9f, 0.9f, 0.9f, 1.0f);
    y += 60.0f * z;

    /* Code example */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "Example Code:", 40.0f * z + px, y, 0.6f * z,
        0.4f, 1.0f, 0.6f, 1.0f);
    y += 40.0f * z;

    const char *code_lines[] = {
        "Agentite_SDFFontGenConfig config = AGENTITE_SDF_FONT_GEN_CONFIG_DEFAULT;",
        "config.atlas_width = 512;",
        "config.glyph_scale = 48.0f;",
        "config.generate_msdf = true;",
        "",
        "Agentite_SDFFont *font = agentite_sdf_font_generate(",
        "    text_renderer, \"font.ttf\", &config);"
    };

    for (int i = 0; i < 7; i++) {
        agentite_sdf_text_draw_colored(text, runtime_font,
            code_lines[i], 60.0f * z + px, y, 0.4f * z,
            0.7f, 0.8f, 0.9f, 1.0f);
        y += 25.0f * z;
    }
    y += 30.0f * z;

    /* Generated font demo */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "This text is rendered with a runtime-generated MSDF font!", 40.0f * z + px, y, 0.6f * z,
        1.0f, 0.8f, 0.4f, 1.0f);
    y += 50.0f * z;

    /* Show it works with effects too - need batch breaks for different effects */
    float effect_time = state->animate_effects ? state->time : 0.0f;

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_outline(text, 0.2f, 0.3f, 0.7f, 1.0f, 1.0f);  /* Thicker blue outline */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "With Outline", 60.0f * z + px, y, 0.8f * z,
        1.0f, 1.0f, 1.0f, 1.0f);
    y += 60.0f * z;

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    float glow = 0.25f + 0.1f * sinf(effect_time * 3.0f);
    agentite_sdf_text_set_glow(text, glow, 1.0f, 0.4f, 0.8f, 1.0f);  /* Full alpha pink glow */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "With Glow", 60.0f * z + px, y, 0.8f * z,
        1.0f, 0.8f, 0.9f, 1.0f);
    y += 60.0f * z;

    /* Shadow effect - light color so visible on dark background */
    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);
    agentite_sdf_text_set_shadow(text, 5.0f, 5.0f, 0.4f, 0.5f, 0.5f, 0.6f, 0.9f);  /* Light purple shadow */
    agentite_sdf_text_draw_colored(text, runtime_font,
        "With Shadow", 60.0f * z + px, y, 0.8f * z,
        1.0f, 0.95f, 0.8f, 1.0f);

    agentite_text_end(text);
    agentite_text_begin(text);
    agentite_sdf_text_clear_effects(text);

    /* Config info */
    y += 80.0f * z;
    agentite_sdf_text_draw_colored(text, runtime_font,
        "Configuration used for this demo:", 40.0f * z + px, y, 0.5f * z,
        0.6f, 0.6f, 0.6f, 1.0f);
    y += 30.0f * z;

    /* Display actual config values used */
    char config_lines2[5][64];
    snprintf(config_lines2[0], sizeof(config_lines2[0]), "Atlas: 512x512 pixels");
    snprintf(config_lines2[1], sizeof(config_lines2[1]), "Glyph scale: 48.0 pixels");
    snprintf(config_lines2[2], sizeof(config_lines2[2]), "Pixel range: 4.0");
    snprintf(config_lines2[3], sizeof(config_lines2[3]), "Character set: ASCII (32-126)");
    snprintf(config_lines2[4], sizeof(config_lines2[4]), "Generation time: %llu ms", (unsigned long long)state->gen_time_ms);

    for (int i = 0; i < 5; i++) {
        float r = (i == 4) ? 0.4f : 0.7f;  /* Highlight generation time in green */
        float g = (i == 4) ? 1.0f : 0.7f;
        float b = (i == 4) ? 0.6f : 0.7f;
        agentite_sdf_text_draw_colored(text, runtime_font,
            config_lines2[i], 60.0f * z + px, y, 0.45f * z,
            r, g, b, 1.0f);
        y += 25.0f * z;
    }
}
