/**
 * HiDPI Rendering Bug - Minimal Reproduction
 *
 * This example isolates the postprocess/render-to-texture positioning bug
 * on HiDPI displays. It strips away everything except:
 *   - One sprite at a known position
 *   - Toggle between direct render vs render-to-texture
 *   - Debug markers showing expected vs actual position
 *
 * Controls:
 *   SPACE  - Toggle between direct render and render-to-texture
 *   1-4    - Move sprite to corners (to test different positions)
 *   C      - Center sprite
 *   ESC    - Quit
 *
 * Expected: Sprite appears at the same position in both modes.
 * Bug: On HiDPI, render-to-texture mode shows sprite in wrong position.
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/shader.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;
static const int SPRITE_SIZE = 200;

typedef struct AppState {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    /* For render-to-texture mode */
    Agentite_ShaderSystem *shaders;
    Agentite_PostProcess *postprocess;

    /* Textures */
    Agentite_Texture *test_sprite;   /* Main test sprite */
    Agentite_Texture *marker_tex;    /* Small marker for reference points */
    Agentite_Texture *red_pixel;     /* For border lines */
    Agentite_Texture *cyan_pixel;    /* For center crosshairs */
    Agentite_Texture *yellow_pixel;  /* For quarter markers */

    /* State */
    bool use_render_to_texture;
    float sprite_x;
    float sprite_y;
} AppState;

/* Create a 1x1 solid color texture for drawing lines */
static Agentite_Texture *create_pixel(Agentite_SpriteRenderer *sr,
                                       unsigned char r, unsigned char g,
                                       unsigned char b, unsigned char a) {
    unsigned char pixels[4] = { r, g, b, a };
    return agentite_texture_create(sr, 1, 1, pixels);
}

/* Create a solid colored texture with a border */
static Agentite_Texture *create_test_sprite(Agentite_SpriteRenderer *sr, int size) {
    unsigned char *pixels = (unsigned char *)calloc(size * size * 4, 1);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            bool is_border = (x < 4 || x >= size - 4 || y < 4 || y >= size - 4);

            if (is_border) {
                /* White border */
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            } else {
                /* Blue fill with gradient */
                pixels[idx + 0] = 50 + (x * 100 / size);   /* R */
                pixels[idx + 1] = 100 + (y * 100 / size);  /* G */
                pixels[idx + 2] = 200;                      /* B */
                pixels[idx + 3] = 255;
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* Create a small bright marker texture */
static Agentite_Texture *create_marker(Agentite_SpriteRenderer *sr) {
    int size = 16;
    unsigned char *pixels = (unsigned char *)calloc(size * size * 4, 1);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            /* Bright yellow/green crosshair pattern */
            bool is_cross = (x == size/2 || y == size/2);
            if (is_cross) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 0;
                pixels[idx + 3] = 255;
            } else {
                pixels[idx + 0] = 0;
                pixels[idx + 1] = 100;
                pixels[idx + 2] = 0;
                pixels[idx + 3] = 128;
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

static void print_debug_info(AppState *app) {
    int phys_w, phys_h;
    agentite_get_drawable_size(app->engine, &phys_w, &phys_h);
    float dpi = agentite_get_dpi_scale(app->engine);

    printf("\n=== HiDPI Test Debug Info ===\n");
    printf("Logical size:  %d x %d\n", WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("Physical size: %d x %d\n", phys_w, phys_h);
    printf("DPI scale:     %.2f\n", dpi);
    printf("Sprite pos:    (%.0f, %.0f)\n", app->sprite_x, app->sprite_y);
    printf("Sprite size:   %d x %d\n", SPRITE_SIZE, SPRITE_SIZE);
    printf("Mode:          %s\n", app->use_render_to_texture ? "RENDER-TO-TEXTURE" : "DIRECT");
    printf("=============================\n\n");
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};

    /* Center the sprite initially - sprite uses centered origin (0.5, 0.5)
     * so the position IS the center, not top-left */
    app.sprite_x = WINDOW_WIDTH / 2.0f;
    app.sprite_y = WINDOW_HEIGHT / 2.0f;

    Agentite_Config config = {
        .window_title = "HiDPI Bug Test - SPACE to toggle mode",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    app.engine = agentite_init(&config);
    if (!app.engine) {
        printf("ERROR: Failed to init engine\n");
        return 1;
    }

    SDL_GPUDevice *gpu = agentite_get_gpu_device(app.engine);
    SDL_Window *window = agentite_get_window(app.engine);

    app.sprites = agentite_sprite_init(gpu, window);
    app.input = agentite_input_init();
    app.text = agentite_text_init(gpu, window);

    if (!app.sprites || !app.input) {
        printf("ERROR: Failed to init subsystems\n");
        agentite_shutdown(app.engine);
        return 1;
    }

    if (app.text) {
        /* Use larger font size for HiDPI readability */
        app.font = agentite_font_load(app.text, "assets/fonts/Roboto-Regular.ttf", 32);
    }

    /* Create shader system for render-to-texture mode */
    app.shaders = agentite_shader_system_create(gpu);
    if (!app.shaders) {
        printf("ERROR: Failed to create shader system\n");
        agentite_shutdown(app.engine);
        return 1;
    }

    /* Create postprocess pipeline at LOGICAL dimensions */
    Agentite_PostProcessConfig pp_cfg = AGENTITE_POSTPROCESS_CONFIG_DEFAULT;
    pp_cfg.width = WINDOW_WIDTH;
    pp_cfg.height = WINDOW_HEIGHT;
    app.postprocess = agentite_postprocess_create(app.shaders, window, &pp_cfg);
    if (!app.postprocess) {
        printf("WARNING: Failed to create postprocess - render-to-texture mode disabled\n");
    }

    /* Create test textures */
    app.test_sprite = create_test_sprite(app.sprites, SPRITE_SIZE);
    app.marker_tex = create_marker(app.sprites);
    app.red_pixel = create_pixel(app.sprites, 255, 80, 80, 255);
    app.cyan_pixel = create_pixel(app.sprites, 80, 255, 255, 255);
    app.yellow_pixel = create_pixel(app.sprites, 255, 255, 80, 128);

    if (!app.test_sprite || !app.marker_tex || !app.red_pixel) {
        printf("ERROR: Failed to create textures\n");
        agentite_shutdown(app.engine);
        return 1;
    }

    print_debug_info(&app);

    printf("Controls:\n");
    printf("  SPACE - Toggle direct vs render-to-texture\n");
    printf("  1-4   - Move sprite to corners\n");
    printf("  C     - Center sprite\n");
    printf("  D     - Print debug info\n");
    printf("  ESC   - Quit\n\n");

    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);

        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) agentite_quit(app.engine);
        }
        agentite_input_update(app.input);

        /* Handle input */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_SPACE)) {
            app.use_render_to_texture = !app.use_render_to_texture;
            printf("Mode: %s\n", app.use_render_to_texture ? "RENDER-TO-TEXTURE" : "DIRECT");
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_D)) {
            print_debug_info(&app);
        }

        /* Position presets - sprite uses centered origin (0.5, 0.5), so position
         * is where the CENTER of the sprite is placed, not the top-left corner.
         * To get 'margin' pixels from edge, center must be at margin + half_size. */
        float margin = 50.0f;
        float half_size = SPRITE_SIZE / 2.0f;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1)) {
            app.sprite_x = margin + half_size;
            app.sprite_y = margin + half_size;
            printf("Sprite: top-left (%.0f, %.0f)\n", app.sprite_x, app.sprite_y);
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2)) {
            app.sprite_x = WINDOW_WIDTH - margin - half_size;
            app.sprite_y = margin + half_size;
            printf("Sprite: top-right (%.0f, %.0f)\n", app.sprite_x, app.sprite_y);
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3)) {
            app.sprite_x = margin + half_size;
            app.sprite_y = WINDOW_HEIGHT - margin - half_size;
            printf("Sprite: bottom-left (%.0f, %.0f)\n", app.sprite_x, app.sprite_y);
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_4)) {
            app.sprite_x = WINDOW_WIDTH - margin - half_size;
            app.sprite_y = WINDOW_HEIGHT - margin - half_size;
            printf("Sprite: bottom-right (%.0f, %.0f)\n", app.sprite_x, app.sprite_y);
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_C)) {
            app.sprite_x = WINDOW_WIDTH / 2.0f;
            app.sprite_y = WINDOW_HEIGHT / 2.0f;
            printf("Sprite: center (%.0f, %.0f)\n", app.sprite_x, app.sprite_y);
        }

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (!cmd) continue;

        /* Prepare sprite batch */
        agentite_sprite_begin(app.sprites, NULL);

        /* Draw main test sprite */
        Agentite_Sprite sprite = agentite_sprite_from_texture(app.test_sprite);
        agentite_sprite_draw(app.sprites, &sprite, app.sprite_x, app.sprite_y);

        /* Draw corner markers at expected sprite corners.
         * Both sprite and marker use origin (0.5, 0.5) - they're centered on their draw position.
         * Sprite center is at (sprite_x, sprite_y), so corners are offset by half the sprite size.
         * Marker is also centered, so draw it directly at the corner position. */
        Agentite_Sprite marker = agentite_sprite_from_texture(app.marker_tex);
        float tl_x = app.sprite_x - half_size;  /* Top-left corner X */
        float tl_y = app.sprite_y - half_size;  /* Top-left corner Y */
        agentite_sprite_draw(app.sprites, &marker, tl_x, tl_y);                                    /* TL */
        agentite_sprite_draw(app.sprites, &marker, tl_x + SPRITE_SIZE, tl_y);                      /* TR */
        agentite_sprite_draw(app.sprites, &marker, tl_x, tl_y + SPRITE_SIZE);                      /* BL */
        agentite_sprite_draw(app.sprites, &marker, tl_x + SPRITE_SIZE, tl_y + SPRITE_SIZE);        /* BR */

        /* Draw reference lines showing logical coordinate boundaries */
        const float LINE_W = 3.0f;

        /* RED: Screen border at logical edges (should be at window edges if coordinates are correct) */
        Agentite_Sprite red_line = agentite_sprite_from_texture(app.red_pixel);
        agentite_sprite_draw_scaled(app.sprites, &red_line, 0, 0, LINE_W, (float)WINDOW_HEIGHT);                    /* Left edge */
        agentite_sprite_draw_scaled(app.sprites, &red_line, WINDOW_WIDTH - LINE_W, 0, LINE_W, (float)WINDOW_HEIGHT); /* Right edge */
        agentite_sprite_draw_scaled(app.sprites, &red_line, 0, 0, (float)WINDOW_WIDTH, LINE_W);                      /* Top edge */
        agentite_sprite_draw_scaled(app.sprites, &red_line, 0, WINDOW_HEIGHT - LINE_W, (float)WINDOW_WIDTH, LINE_W); /* Bottom edge */

        /* CYAN: Center crosshairs at (640, 360) - full screen span */
        Agentite_Sprite cyan_line = agentite_sprite_from_texture(app.cyan_pixel);
        float cx = WINDOW_WIDTH / 2.0f;
        float cy = WINDOW_HEIGHT / 2.0f;
        agentite_sprite_draw_scaled(app.sprites, &cyan_line, 0, cy - LINE_W/2, (float)WINDOW_WIDTH, LINE_W);   /* Full horizontal line */
        agentite_sprite_draw_scaled(app.sprites, &cyan_line, cx - LINE_W/2, 0, LINE_W, (float)WINDOW_HEIGHT);  /* Full vertical line */

        /* YELLOW: Quarter markers */
        Agentite_Sprite yellow_line = agentite_sprite_from_texture(app.yellow_pixel);
        float qw = WINDOW_WIDTH / 4.0f;
        float qh = WINDOW_HEIGHT / 4.0f;
        /* Vertical quarter lines */
        agentite_sprite_draw_scaled(app.sprites, &yellow_line, qw, 0, 1, (float)WINDOW_HEIGHT);
        agentite_sprite_draw_scaled(app.sprites, &yellow_line, qw * 3, 0, 1, (float)WINDOW_HEIGHT);
        /* Horizontal quarter lines */
        agentite_sprite_draw_scaled(app.sprites, &yellow_line, 0, qh, (float)WINDOW_WIDTH, 1);
        agentite_sprite_draw_scaled(app.sprites, &yellow_line, 0, qh * 3, (float)WINDOW_WIDTH, 1);

        /* Prepare text */
        if (app.text && app.font) {
            int phys_w, phys_h;
            agentite_get_drawable_size(app.engine, &phys_w, &phys_h);
            float dpi = agentite_get_dpi_scale(app.engine);

            agentite_text_begin(app.text);

            /* Mode indicator */
            const char *mode = app.use_render_to_texture ? "RENDER-TO-TEXTURE (grayscale)" : "DIRECT";
            float r = app.use_render_to_texture ? 1.0f : 0.5f;
            float g = app.use_render_to_texture ? 0.5f : 1.0f;
            agentite_text_draw_colored(app.text, app.font, mode, 20, 20, r, g, 0.5f, 1.0f);

            /* Debug info */
            char buf[128];
            snprintf(buf, sizeof(buf), "Logical: %dx%d  Physical: %dx%d  DPI: %.1f",
                     WINDOW_WIDTH, WINDOW_HEIGHT, phys_w, phys_h, dpi);
            agentite_text_draw_colored(app.text, app.font, buf, 20, 60, 0.7f, 0.7f, 0.7f, 1.0f);

            snprintf(buf, sizeof(buf), "Sprite pos: (%.0f, %.0f)  size: %dx%d",
                     app.sprite_x, app.sprite_y, SPRITE_SIZE, SPRITE_SIZE);
            agentite_text_draw_colored(app.text, app.font, buf, 20, 100, 0.7f, 0.7f, 0.7f, 1.0f);

            /* Legend for debug visuals */
            agentite_text_draw_colored(app.text, app.font,
                "RED = screen edges (should touch window border)",
                20, 160, 1.0f, 0.3f, 0.3f, 1.0f);
            agentite_text_draw_colored(app.text, app.font,
                "CYAN = center crosshair (640,360)",
                20, 195, 0.3f, 1.0f, 1.0f, 1.0f);
            agentite_text_draw_colored(app.text, app.font,
                "YELLOW = quarter grid lines",
                20, 230, 1.0f, 1.0f, 0.3f, 1.0f);
            agentite_text_draw_colored(app.text, app.font,
                "GREEN markers = sprite corners",
                20, 265, 0.3f, 1.0f, 0.3f, 1.0f);

            /* Bug explanation */
            agentite_text_draw_colored(app.text, app.font,
                "BUG: If red lines don't touch window edges,",
                20, 320, 1.0f, 0.7f, 0.7f, 1.0f);
            agentite_text_draw_colored(app.text, app.font,
                "viewport is using wrong dimensions (logical vs physical)",
                20, 355, 1.0f, 0.7f, 0.7f, 1.0f);

            /* Controls */
            agentite_text_draw_colored(app.text, app.font,
                "SPACE: Toggle mode | 1-4: Corners | C: Center | ESC: Quit",
                20, WINDOW_HEIGHT - 50, 0.5f, 0.5f, 0.5f, 1.0f);

            agentite_text_end(app.text);
        }

        /* Upload ALL data BEFORE render pass */
        agentite_sprite_upload(app.sprites, cmd);
        if (app.text) agentite_text_upload(app.text, cmd);

        if (app.use_render_to_texture && app.postprocess) {
            /* === RENDER-TO-TEXTURE PATH === */
            SDL_GPUTexture *pp_target = agentite_postprocess_get_target(app.postprocess);

            /* Pass 1: Render sprites to texture */
            if (agentite_begin_render_pass_to_texture(app.engine, pp_target,
                    WINDOW_WIDTH, WINDOW_HEIGHT, 0.2f, 0.1f, 0.1f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                agentite_end_render_pass_no_submit(app.engine);
            }

            /* Pass 2: Blit texture to swapchain with grayscale effect
             * (Using grayscale since SHADER_NONE has no actual shader) */
            if (agentite_begin_render_pass(app.engine, 0.0f, 0.0f, 0.0f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);

                int phys_w, phys_h;
                agentite_get_drawable_size(app.engine, &phys_w, &phys_h);

                /* Use grayscale shader - simple effect that doesn't affect positioning */
                Agentite_Shader *effect = agentite_shader_get_builtin(app.shaders, AGENTITE_SHADER_GRAYSCALE);
                if (effect) {
                    agentite_postprocess_begin(app.postprocess, cmd, pp_target);
                    agentite_postprocess_apply_scaled(app.postprocess, cmd, pass, effect, NULL, phys_w, phys_h);
                    agentite_postprocess_end(app.postprocess, cmd, pass);
                }

                /* Render text on top (not affected by postprocess) */
                if (app.text) agentite_text_render(app.text, cmd, pass);

                agentite_end_render_pass(app.engine);
            }
        } else {
            /* === DIRECT RENDER PATH === */
            if (agentite_begin_render_pass(app.engine, 0.1f, 0.2f, 0.1f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                if (app.text) agentite_text_render(app.text, cmd, pass);
                agentite_end_render_pass(app.engine);
            }
        }

        agentite_end_frame(app.engine);
    }

    /* Cleanup */
    SDL_WaitForGPUIdle(gpu);

    if (app.test_sprite) agentite_texture_destroy(app.sprites, app.test_sprite);
    if (app.marker_tex) agentite_texture_destroy(app.sprites, app.marker_tex);
    if (app.red_pixel) agentite_texture_destroy(app.sprites, app.red_pixel);
    if (app.cyan_pixel) agentite_texture_destroy(app.sprites, app.cyan_pixel);
    if (app.yellow_pixel) agentite_texture_destroy(app.sprites, app.yellow_pixel);
    if (app.postprocess) agentite_postprocess_destroy(app.postprocess);
    if (app.shaders) agentite_shader_system_destroy(app.shaders);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
